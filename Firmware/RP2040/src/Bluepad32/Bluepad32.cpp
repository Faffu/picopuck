#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <functional>
#include <pico/mutex.h>
#include <pico/time.h>
#include <pico/cyw43_arch.h>

#include "btstack_run_loop.h"
#include "uni.h"
extern "C" {
#include "parser/uni_hid_parser_steam.h"
#include "bt/uni_bt_le.h"
}

#include "sdkconfig.h"
#include "Bluepad32/Bluepad32.h"
#include "SteamController2/SteamController2.h"
#include "SteamController2/ReportCapture.h"
#include "Board/board_api.h"
#include "Board/ogxm_log.h"

#ifndef CONFIG_BLUEPAD32_PLATFORM_CUSTOM
    #error "Pico W must use BLUEPAD32_PLATFORM_CUSTOM"
#endif

static_assert((CONFIG_BLUEPAD32_MAX_DEVICES == MAX_GAMEPADS), "Mismatch between BP32 and Gamepad max devices");

namespace bluepad32 {

//The feedback timer ticks at the Steam Controller 2 rumble refresh rate, other
//controllers get a longer rumble request every FEEDBACK_TICKS ticks.
static constexpr uint32_t FEEDBACK_TICK_MS = SteamController2::RUMBLE_RESEND_MS;
static constexpr uint32_t FEEDBACK_TICKS = 6;
static constexpr uint32_t FEEDBACK_TIME_MS = FEEDBACK_TICK_MS * FEEDBACK_TICKS;

static constexpr uint16_t VALVE_VID = 0x28de;
static constexpr uint16_t SC2_BLE_PID = 0x1303; //HID over GATT, vendor page 0xff00

static bool is_steam_controller_2(const uni_hid_device_t* device)
{
    return device->vendor_id == VALVE_VID && device->product_id == SC2_BLE_PID;
}

static void steam_controller_2_rumble(uni_hid_device_t* device, uint8_t strong, uint8_t weak)
{
    //Static: the GATT write keeps a pointer to it until the write goes out.
    static uint8_t report[SteamController2::RUMBLE_REPORT_LEN];
    SteamController2::encode_rumble(strong, weak, report);
    //Busy with the previous write is fine, the next tick sends a fresh one.
    hids_client_send_write_report(device->hids_cid, SteamController2::RUMBLE_REPORT_ID,
                                  HID_REPORT_TYPE_OUTPUT, report, sizeof(report));
}
static constexpr uint32_t LED_CHECK_TIME_MS = 500;

struct BTDevice {
    bool connected{false};
    Gamepad* gamepad{nullptr};
};

BTDevice bt_devices_[MAX_GAMEPADS];
btstack_timer_source_t feedback_timer_;
btstack_timer_source_t led_timer_;
bool led_timer_set_{false};
bool hid_descriptor_logged_{false};
bool feedback_timer_set_{false};

bool any_connected()
{
    for (auto& device : bt_devices_)
    {
        if (device.connected)
        {
            return true;
        }
    }
    return false;
}

//This solves a function pointer/crash issue with bluepad32
void set_rumble(uni_hid_device_t* bp_device, uint16_t length, uint8_t rumble_l, uint8_t rumble_r)
{
    switch (bp_device->controller_type)
    {
        case CONTROLLER_TYPE_XBoxOneController:
            uni_hid_parser_xboxone_play_dual_rumble(bp_device, 0, length + 10, rumble_l, rumble_r);
            break;
        case CONTROLLER_TYPE_AndroidController:
            if (bp_device->vendor_id == UNI_HID_PARSER_STADIA_VID && bp_device->product_id == UNI_HID_PARSER_STADIA_PID) 
            {
                uni_hid_parser_stadia_play_dual_rumble(bp_device, 0, length, rumble_l, rumble_r);
            }
            break;
        case CONTROLLER_TYPE_PSMoveController:
            uni_hid_parser_psmove_play_dual_rumble(bp_device, 0, length, rumble_l, rumble_r);
            break;
        case CONTROLLER_TYPE_PS3Controller:
            uni_hid_parser_ds3_play_dual_rumble(bp_device, 0, length, rumble_l, rumble_r);
            break;
        case CONTROLLER_TYPE_PS4Controller:
            uni_hid_parser_ds4_play_dual_rumble(bp_device, 0, length, rumble_l, rumble_r);
            break;
        case CONTROLLER_TYPE_PS5Controller:
            uni_hid_parser_ds5_play_dual_rumble(bp_device, 0, length, rumble_l, rumble_r);
            break;
        case CONTROLLER_TYPE_WiiController:
            uni_hid_parser_wii_play_dual_rumble(bp_device, 0, length, rumble_l, rumble_r);
            break;
        case CONTROLLER_TYPE_SteamController:
        case CONTROLLER_TYPE_SteamControllerV2:
            uni_hid_parser_steam_play_dual_rumble(bp_device, length, rumble_l, rumble_r);
            break;
        case CONTROLLER_TYPE_SwitchProController:
        case CONTROLLER_TYPE_SwitchJoyConRight:
        case CONTROLLER_TYPE_SwitchJoyConLeft:
            uni_hid_parser_switch_play_dual_rumble(bp_device, 0, length, rumble_l, rumble_r);
            break;
        default:
            break;
    }
}

static void send_feedback_cb(btstack_timer_source *ts)
{
    static uint32_t tick = 0;
    const bool slow_tick = (tick++ % FEEDBACK_TICKS) == 0;

    uni_hid_device_t* bp_device = nullptr;

    for (uint8_t i = 0; i < MAX_GAMEPADS; ++i)
    {
        if (!bt_devices_[i].connected || 
            !(bp_device = uni_hid_device_get_instance_for_idx(i)))
        {
            continue;
        }

        Gamepad::PadOut gp_out = bt_devices_[i].gamepad->get_pad_out();
        if (gp_out.rumble_l == 0 && gp_out.rumble_r == 0)
        {
            continue; //every controller stops on its own once requests end
        }
        if (is_steam_controller_2(bp_device))
        {
            steam_controller_2_rumble(bp_device, gp_out.rumble_l, gp_out.rumble_r);
        }
        else if (slow_tick)
        {
            set_rumble(bp_device, static_cast<uint16_t>(FEEDBACK_TIME_MS), gp_out.rumble_l, gp_out.rumble_r);
        }
    }

    btstack_run_loop_set_timer(ts, FEEDBACK_TICK_MS);
    btstack_run_loop_add_timer(ts);
}

static void check_led_cb(btstack_timer_source *ts)
{
    static bool led_state = false;

    led_state = !led_state;

    board_api::set_led(any_connected() ? true : led_state);

    btstack_run_loop_set_timer(ts, LED_CHECK_TIME_MS);
    btstack_run_loop_add_timer(ts);
}

//BT Driver

static void init(int argc, const char** arg_V) {
}

static void init_complete_cb(void) {
    uni_bt_enable_new_connections_unsafe(true);
    // uni_bt_del_keys_unsafe();
    uni_property_dump_all();
#if defined(CONFIG_OGXM_FORCE_CAPTURE)
    uni_bt_le_list_bonded_keys(); //shows whether bonds survived the reboot
#endif
}

static uni_error_t device_discovered_cb(bd_addr_t addr, const char* name, uint16_t cod, uint8_t rssi) {
    //BLE controllers advertise as gamepad or joystick, accept both.
    const uint16_t minor = cod & UNI_BT_COD_MINOR_MASK;
    if (!(minor & (UNI_BT_COD_MINOR_GAMEPAD | UNI_BT_COD_MINOR_JOYSTICK))) {
        return UNI_ERROR_IGNORE_DEVICE;
    }
    return UNI_ERROR_SUCCESS;
}

static void device_connected_cb(uni_hid_device_t* device) {
}

static void device_disconnected_cb(uni_hid_device_t* device) {
    int idx = uni_hid_device_get_idx_for_instance(device);
    if (idx >= MAX_GAMEPADS || idx < 0) {
        return;
    }

    bt_devices_[idx].connected = false;
    hid_descriptor_logged_ = false;
    bt_devices_[idx].gamepad->reset_pad_in();
    bt_devices_[idx].gamepad->reset_pad_motion();
    bt_devices_[idx].gamepad->reset_pad_out();

    if (!led_timer_set_ && !any_connected()) {
        led_timer_set_ = true;
        led_timer_.process = check_led_cb;
        led_timer_.context = nullptr;
        btstack_run_loop_set_timer(&led_timer_, LED_CHECK_TIME_MS);
        btstack_run_loop_add_timer(&led_timer_);
    }
    if (feedback_timer_set_ && !any_connected()) {
        feedback_timer_set_ = false;
        btstack_run_loop_remove_timer(&feedback_timer_);
    }
}

static uni_error_t device_ready_cb(uni_hid_device_t* device) {    
    int idx = uni_hid_device_get_idx_for_instance(device);
    if (idx >= MAX_GAMEPADS || idx < 0) {
        return UNI_ERROR_SUCCESS;
    }

    bt_devices_[idx].connected = true;

    if (led_timer_set_) {
        led_timer_set_ = false;
        btstack_run_loop_remove_timer(&led_timer_);
        board_api::set_led(true);
    }
    if (!feedback_timer_set_) {
        feedback_timer_set_ = true;
        feedback_timer_.process = send_feedback_cb;
        feedback_timer_.context = nullptr;
        btstack_run_loop_set_timer(&feedback_timer_, FEEDBACK_TICK_MS);
        btstack_run_loop_add_timer(&feedback_timer_);
    }
    return UNI_ERROR_SUCCESS;
}

static void oob_event_cb(uni_platform_oob_event_t event, void* data) {
	return;
}

static void controller_data_cb(uni_hid_device_t* device, uni_controller_t* controller) {
    static uni_gamepad_t prev_uni_gp[MAX_GAMEPADS] = {};

    if (controller->klass != UNI_CONTROLLER_CLASS_GAMEPAD){
        return;
    }
    //The Steam Controller 2 state is written by ogxm_steam_parse_input_report.
    //Bluepad32 still calls this after every report with its own, empty, parser
    //state, which would zero PadIn every other update: flickering input and
    //mode combos that never hold for three seconds.
    if (is_steam_controller_2(device)) {
        return;
    }

    uni_gamepad_t *uni_gp = &controller->gamepad;
    int idx = uni_hid_device_get_idx_for_instance(device);

    Gamepad* gamepad = bt_devices_[idx].gamepad;
    Gamepad::PadIn gp_in;

    switch (uni_gp->dpad) 
    {
        case DPAD_UP:
            gp_in.dpad = gamepad->MAP_DPAD_UP;
            break;
        case DPAD_DOWN:
            gp_in.dpad = gamepad->MAP_DPAD_DOWN;
            break;
        case DPAD_LEFT:
            gp_in.dpad = gamepad->MAP_DPAD_LEFT;
            break;
        case DPAD_RIGHT:
            gp_in.dpad = gamepad->MAP_DPAD_RIGHT;
            break;
        case DPAD_UP | DPAD_RIGHT:
            gp_in.dpad = gamepad->MAP_DPAD_UP_RIGHT;
            break;
        case DPAD_DOWN | DPAD_RIGHT:
            gp_in.dpad = gamepad->MAP_DPAD_DOWN_RIGHT;
            break;
        case DPAD_DOWN | DPAD_LEFT:
            gp_in.dpad = gamepad->MAP_DPAD_DOWN_LEFT;
            break;
        case DPAD_UP | DPAD_LEFT:
            gp_in.dpad = gamepad->MAP_DPAD_UP_LEFT;
            break;
        default:
            break;
    }

    if (uni_gp->buttons & BUTTON_A) gp_in.buttons |= gamepad->MAP_BUTTON_A;
    if (uni_gp->buttons & BUTTON_B) gp_in.buttons |= gamepad->MAP_BUTTON_B;
    if (uni_gp->buttons & BUTTON_X) gp_in.buttons |= gamepad->MAP_BUTTON_X;
    if (uni_gp->buttons & BUTTON_Y) gp_in.buttons |= gamepad->MAP_BUTTON_Y;
    if (uni_gp->buttons & BUTTON_SHOULDER_L) gp_in.buttons |= gamepad->MAP_BUTTON_LB;
    if (uni_gp->buttons & BUTTON_SHOULDER_R) gp_in.buttons |= gamepad->MAP_BUTTON_RB;
    if (uni_gp->buttons & BUTTON_THUMB_L)    gp_in.buttons |= gamepad->MAP_BUTTON_L3;  
    if (uni_gp->buttons & BUTTON_THUMB_R)    gp_in.buttons |= gamepad->MAP_BUTTON_R3;
    if (uni_gp->misc_buttons & MISC_BUTTON_BACK)    gp_in.buttons |= gamepad->MAP_BUTTON_BACK;
    if (uni_gp->misc_buttons & MISC_BUTTON_START)   gp_in.buttons |= gamepad->MAP_BUTTON_START;
    if (uni_gp->misc_buttons & MISC_BUTTON_SYSTEM)  gp_in.buttons |= gamepad->MAP_BUTTON_SYS;

    gp_in.trigger_l = gamepad->scale_trigger_l<10>(static_cast<uint16_t>(uni_gp->brake));
    gp_in.trigger_r = gamepad->scale_trigger_r<10>(static_cast<uint16_t>(uni_gp->throttle));
    
    std::tie(gp_in.joystick_lx, gp_in.joystick_ly) = gamepad->scale_joystick_l<10>(uni_gp->axis_x, uni_gp->axis_y);
    std::tie(gp_in.joystick_rx, gp_in.joystick_ry) = gamepad->scale_joystick_r<10>(uni_gp->axis_rx, uni_gp->axis_ry);

    gamepad->set_pad_in(gp_in);
}

#if defined(CONFIG_OGXM_FORCE_CAPTURE)
//Overrides bluepad32's weak logger so pairing and discovery show up on the
//capture serial port. Runs on core1 like every other capture producer.
extern "C" void uni_logv(const char* fmt, va_list args)
{
    char text[160];
    const int len = std::vsnprintf(text, sizeof(text), fmt, args);
    if (len > 0)
    {
        ReportCapture::push_text(text, std::min<size_t>(static_cast<size_t>(len), sizeof(text) - 1));
    }
}
#endif

//Called by bluepad32 for every BLE advertising report, before its own filter
//(appearance must be gamepad or joystick). The capture build logs reports a
//reconnecting controller could be sending: directed advertising, anything with
//an appearance, and anything carrying Valve's company id. Each distinct report
//is logged once.
extern "C" void ogxm_adv_report_hook(const uint8_t* packet)
{
#if defined(CONFIG_OGXM_FORCE_CAPTURE)
    const uint8_t event_type = gap_event_advertising_report_get_advertising_event_type(packet);
    const uint8_t data_len = gap_event_advertising_report_get_data_length(packet);
    const uint8_t* data = gap_event_advertising_report_get_data(packet);

    bool interesting = (event_type == 1); //ADV_DIRECT_IND
    for (uint8_t i = 0; i + 1 < data_len && data[i] != 0; i = static_cast<uint8_t>(i + data[i] + 1))
    {
        const uint8_t ad_type = data[i + 1];
        if (ad_type == BLUETOOTH_DATA_TYPE_APPEARANCE)
        {
            interesting = true;
        }
        if (ad_type == BLUETOOTH_DATA_TYPE_MANUFACTURER_SPECIFIC_DATA && data[i] >= 3 &&
            i + 3 < data_len && data[i + 2] == 0x5d && data[i + 3] == 0x05)
        {
            interesting = true;
        }
    }
    if (!interesting)
    {
        return;
    }

    bd_addr_t addr;
    gap_event_advertising_report_get_address(packet, addr);
    uint32_t signature = event_type ^ (static_cast<uint32_t>(data_len) << 8);
    for (uint8_t i = 0; i < 6; ++i)
    {
        signature = signature * 31 + addr[i];
    }
    for (uint8_t i = 0; i < data_len; ++i)
    {
        signature = signature * 31 + data[i];
    }
    static uint32_t seen[16] = {};
    static uint8_t next = 0;
    for (uint32_t s : seen)
    {
        if (s == signature)
        {
            return;
        }
    }
    seen[next] = signature;
    next = static_cast<uint8_t>((next + 1) % 16);

    char hex[3 * 31 + 1] = {};
    for (uint8_t i = 0; i < data_len && i < 31; ++i)
    {
        std::snprintf(&hex[3 * i], 4, "%02x ", data[i]);
    }
    uni_log("# adv type %u addr %s (addr type %u) rssi %d data %s\n", event_type, bd_addr_to_str(addr),
            gap_event_advertising_report_get_address_type(packet),
            static_cast<int8_t>(gap_event_advertising_report_get_rssi(packet)), hex);
#else
    (void)packet;
#endif
}

//Steam Controller 2 input, called from ogxm_input_report_hook and from
//bluepad32's Steam parser. Returns true when the report was decoded and applied.
extern "C" bool ogxm_steam_parse_input_report(uni_hid_device_t* device, const uint8_t* report, uint16_t len)
{
    static SteamController2::MotionScale motion_scale;

    SteamController2::State state;
    if (!SteamController2::decode(report, len, state))
    {
        return false; //mouse, keyboard and status reports carry nothing we map
    }

    const int idx = uni_hid_device_get_idx_for_instance(device);
    if (idx < 0 || idx >= MAX_GAMEPADS || !bt_devices_[idx].gamepad)
    {
        return true; //decoded but nowhere to put it
    }

    Gamepad* gamepad = bt_devices_[idx].gamepad;
    Gamepad::PadIn gp_in;

    const uint32_t buttons = state.buttons;

    if (buttons & SteamController2::BTN_A) gp_in.buttons |= gamepad->MAP_BUTTON_A;
    if (buttons & SteamController2::BTN_B) gp_in.buttons |= gamepad->MAP_BUTTON_B;
    if (buttons & SteamController2::BTN_X) gp_in.buttons |= gamepad->MAP_BUTTON_X;
    if (buttons & SteamController2::BTN_Y) gp_in.buttons |= gamepad->MAP_BUTTON_Y;
    if (buttons & SteamController2::BTN_SHOULDER_L) gp_in.buttons |= gamepad->MAP_BUTTON_LB;
    if (buttons & SteamController2::BTN_SHOULDER_R) gp_in.buttons |= gamepad->MAP_BUTTON_RB;
    if (buttons & SteamController2::BTN_THUMB_L) gp_in.buttons |= gamepad->MAP_BUTTON_L3;
    if (buttons & SteamController2::BTN_THUMB_R) gp_in.buttons |= gamepad->MAP_BUTTON_R3;
    if (buttons & SteamController2::BTN_VIEW)   gp_in.buttons |= gamepad->MAP_BUTTON_BACK;
    if (buttons & SteamController2::BTN_MENU)   gp_in.buttons |= gamepad->MAP_BUTTON_START;
    if (buttons & SteamController2::BTN_STEAM)  gp_in.buttons |= gamepad->MAP_BUTTON_SYS;
    if (buttons & SteamController2::BTN_QUICK_ACCESS) gp_in.buttons |= gamepad->MAP_BUTTON_MISC;

    const bool up    = buttons & SteamController2::BTN_DPAD_UP;
    const bool down  = buttons & SteamController2::BTN_DPAD_DOWN;
    const bool left  = buttons & SteamController2::BTN_DPAD_LEFT;
    const bool right = buttons & SteamController2::BTN_DPAD_RIGHT;

    if (up)    gp_in.dpad |= gamepad->MAP_DPAD_UP;
    if (down)  gp_in.dpad |= gamepad->MAP_DPAD_DOWN;
    if (left)  gp_in.dpad |= gamepad->MAP_DPAD_LEFT;
    if (right) gp_in.dpad |= gamepad->MAP_DPAD_RIGHT;

    //Digital trigger clicks still count as fully pressed.
    uint8_t trigger_l = state.trigger_l;
    uint8_t trigger_r = state.trigger_r;
    if (buttons & SteamController2::BTN_TRIGGER_L) trigger_l = 0xFF;
    if (buttons & SteamController2::BTN_TRIGGER_R) trigger_r = 0xFF;

    gp_in.trigger_l = gamepad->scale_trigger_l(trigger_l);
    gp_in.trigger_r = gamepad->scale_trigger_r(trigger_r);

    std::tie(gp_in.joystick_lx, gp_in.joystick_ly) =
        gamepad->scale_joystick_l(state.stick_lx, state.stick_ly);

    std::tie(gp_in.joystick_rx, gp_in.joystick_ry) =
        gamepad->scale_joystick_r(state.stick_rx, state.stick_ry);

    gamepad->set_pad_in(gp_in);

    Gamepad::PadMotion motion = gamepad->get_pad_motion();

    motion.accel_x = SteamController2::scale_accel(state.accel_x, motion_scale);
    motion.accel_y = SteamController2::scale_accel(state.accel_y, motion_scale);
    motion.accel_z = SteamController2::scale_accel(state.accel_z, motion_scale);
    motion.gyro_x = SteamController2::scale_gyro(state.gyro_x, motion_scale);
    motion.gyro_y = SteamController2::scale_gyro(state.gyro_y, motion_scale);
    motion.gyro_z = SteamController2::scale_gyro(state.gyro_z, motion_scale);
    motion.trackpad_lx = state.pad_lx;
    motion.trackpad_ly = state.pad_ly;
    motion.trackpad_rx = state.pad_rx;
    motion.trackpad_ry = state.pad_ry;
    motion.back_buttons = state.back_buttons;
    motion.valid = Gamepad::MOTION_VALID_ACCEL | Gamepad::MOTION_VALID_GYRO |
                   Gamepad::MOTION_VALID_PAD_L | Gamepad::MOTION_VALID_PAD_R |
                   Gamepad::MOTION_VALID_BACK;

    gamepad->set_pad_motion(motion);
    return true;
}

const uni_property_t* get_property_cb(uni_property_idx_t idx) 
{
    return nullptr;
}

uni_platform* get_driver() 
{
    static uni_platform driver = 
    {
        .name = "OGXMiniW",
        .init = init,
        .on_init_complete = init_complete_cb,
        .on_device_discovered = device_discovered_cb,
        .on_device_connected = device_connected_cb,
        .on_device_disconnected = device_disconnected_cb,
        .on_device_ready = device_ready_cb,
        .on_controller_data = controller_data_cb,
        .get_property = get_property_cb,
        .on_oob_event = oob_event_cb,
    };
    return &driver;
}

//Called by bluepad32 for every input report of every device, before any parser.
//Returns true when the report is handled here and bluepad32 must not parse it.
extern "C" bool ogxm_input_report_hook(uni_hid_device_t* device, const uint8_t* report, uint16_t len)
{
    //Every report is captured, decodable or not; capture mode prints them.
    ReportCapture::push(report, len, time_us_32());

#if defined(CONFIG_OGXM_FORCE_CAPTURE)
    if (!hid_descriptor_logged_ && device->hid_descriptor_len > 0)
    {
        hid_descriptor_logged_ = true;
        uni_log("# hid descriptor %d bytes\n", device->hid_descriptor_len);
        for (int off = 0; off < device->hid_descriptor_len; off += 16)
        {
            char line[8 + 3 * 16 + 2];
            int n = std::snprintf(line, sizeof(line), "# hd ");
            for (int i = off; i < off + 16 && i < device->hid_descriptor_len; ++i)
            {
                n += std::snprintf(&line[n], sizeof(line) - n, "%02x ", device->hid_descriptor[i]);
            }
            uni_log("%s\n", line);
        }
    }
#endif

    if (!is_steam_controller_2(device))
    {
        return false;
    }
    ogxm_steam_parse_input_report(device, report, len);
    return true; //bluepad32's Android fallback can't map its vendor reports
}

//Public API

void run_task(Gamepad(&gamepads)[MAX_GAMEPADS])
{
    for (uint8_t i = 0; i < MAX_GAMEPADS; ++i)
    {
        bt_devices_[i].gamepad = &gamepads[i];
    }

    uni_platform_set_custom(get_driver());
    uni_init(0, nullptr);

    led_timer_set_ = true;
    led_timer_.process = check_led_cb;
    led_timer_.context = nullptr;
    btstack_run_loop_set_timer(&led_timer_, LED_CHECK_TIME_MS);
    btstack_run_loop_add_timer(&led_timer_);

    btstack_run_loop_execute();
}

} // namespace bluepad32 