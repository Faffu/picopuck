#include <cstring>

#include "pico/time.h"

#include "USBDevice/DeviceDriver/SwitchPro/SwitchPro.h"

namespace {

//SPI flash contents the host reads with subcommand 0x10, byte for byte from
//2wiCC's procon_data.c (MIT, KNfLrPn, https://github.com/knflrpn/2wiCC):
//pairing data at 0x2000, and at 0x6000 the controller type (0x6012 = 3, Pro
//Controller), IMU calibration (accel sensitivity 0x4000, gyro 0x343B), stick
//calibration, colours and stick parameters. Anything else reads as 0xFF.
const uint8_t SPI_0x2000[144] = {
    0x00, 0x22, 0x84, 0x6E, 0xDC, 0x68, 0xEB, 0x69, //0x2000
    0xD4, 0xC2, 0x5C, 0x61, 0x49, 0xE3, 0xDE, 0xAE, //0x2008
    0x18, 0x52, 0x2A, 0x75, 0xD3, 0x53, 0x14, 0xF2, //0x2010
    0xA7, 0xEA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //0x2018
    0x00, 0x00, 0x00, 0x00, 0x68, 0x00, 0x95, 0x22, //0x2020
    0x30, 0xB2, 0x8C, 0xC6, 0x81, 0x1A, 0x3A, 0x4B, //0x2028
    0x1A, 0x1E, 0xBB, 0x5D, 0x1C, 0xC7, 0x41, 0xD9, //0x2030
    0x5E, 0xC8, 0xBA, 0x21, 0x18, 0x9C, 0xAE, 0x3B, //0x2038
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //0x2040
    0x00, 0x00, 0x08, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, //0x2048
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //0x2050
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //0x2058
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //0x2060
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //0x2068
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //0x2070
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //0x2078
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //0x2080
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //0x2088
};

const uint8_t SPI_0x6000[256] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //0x6000
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //0x6008
    0xFF, 0xFF, 0x03, 0xA0, 0xFF, 0xFF, 0xFF, 0xFF, //0x6010
    0xFF, 0xFF, 0xFF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, //0x6018
    0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x40, //0x6020
    0x00, 0x40, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, //0x6028
    0x00, 0x00, 0x3B, 0x34, 0x3B, 0x34, 0x3B, 0x34, //0x6030
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x06, 0x60, //0x6038
    0x00, 0x08, 0x80, 0x00, 0x06, 0x60, 0x00, 0x08, //0x6040
    0x80, 0x00, 0x06, 0x60, 0x00, 0x06, 0x60, 0xFF, //0x6048
    0x42, 0x66, 0x3A, 0xFF, 0xFF, 0xFF, 0x4E, 0x90, //0x6050
    0x40, 0x4E, 0x90, 0x40, 0xFF, 0xFF, 0xFF, 0xFF, //0x6058
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //0x6060
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //0x6068
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //0x6070
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //0x6078
    0x50, 0xFD, 0x00, 0x00, 0xC6, 0x0F, 0x0F, 0x30, //0x6080
    0x61, 0x06, 0x30, 0xF3, 0xD4, 0x14, 0x54, 0x41, //0x6088
    0x15, 0x54, 0xC7, 0x79, 0x9C, 0x33, 0x36, 0x63, //0x6090
    0x0F, 0x30, 0x61, 0x06, 0x30, 0xF3, 0xD4, 0x14, //0x6098
    0x54, 0x41, 0x15, 0x54, 0xC7, 0x79, 0x9C, 0x33, //0x60A0
    0x36, 0x63, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //0x60A8
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //0x60B0
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //0x60B8
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //0x60C0
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //0x60C8
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //0x60D0
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //0x60D8
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //0x60E0
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //0x60E8
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //0x60F0
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //0x60F8
};

constexpr uint8_t CONN_INFO_USB_POWERED = 0x01;
constexpr uint8_t BATTERY_FULL_CHARGING = 0x90;
constexpr uint8_t CHARGING_GRIP = 0x80;  //buttons[1], set by a wired Pro Controller
constexpr uint8_t VIBRATOR_REPORT = 0x09; //what a real controller mostly sends
constexpr uint32_t ANNOUNCE_INTERVAL_US = 250000;

} // namespace

SwitchProCodec::ImuCalibration& SwitchProDevice::imu_calibration()
{
    static SwitchProCodec::ImuCalibration calibration;
    return calibration;
}

uint8_t& SwitchProDevice::rumble_intensity()
{
    static uint8_t intensity = 255;
    return intensity;
}

void SwitchProDevice::initialize()
{
    class_driver_ =
    {
        .name = TUD_DRV_NAME("SWITCH_PRO"),
        .init = hidd_init,
        .deinit = hidd_deinit,
        .reset = hidd_reset,
        .open = hidd_open,
        .control_xfer_cb = hidd_control_xfer_cb,
        .xfer_cb = hidd_xfer_cb,
        .sof = NULL
    };

    in_report_.fill(0);
    pending_reply_.fill(0);
    pending_reply_len_ = 0;
    full_reports_ = false;
}

void SwitchProDevice::fill_pad_state(Gamepad& gamepad, uint8_t* buffer)
{
    //buffer points at the "info" byte, the layout after it is shared by the
    //0x30 input report and the 0x21 subcommand reply.
    Gamepad::PadIn gp_in = gamepad.get_pad_in();

    buffer[0] = BATTERY_FULL_CHARGING | CONN_INFO_USB_POWERED;

    uint8_t buttons[3] = { 0, CHARGING_GRIP, 0 };

    if (gp_in.buttons & Gamepad::BUTTON_A)  buttons[0] |= SwitchPro::Buttons0::B;
    if (gp_in.buttons & Gamepad::BUTTON_B)  buttons[0] |= SwitchPro::Buttons0::A;
    if (gp_in.buttons & Gamepad::BUTTON_X)  buttons[0] |= SwitchPro::Buttons0::Y;
    if (gp_in.buttons & Gamepad::BUTTON_Y)  buttons[0] |= SwitchPro::Buttons0::X;
    if (gp_in.buttons & Gamepad::BUTTON_RB) buttons[0] |= SwitchPro::Buttons0::R;
    if (gp_in.trigger_r)                    buttons[0] |= SwitchPro::Buttons0::ZR;

    if (gp_in.buttons & Gamepad::BUTTON_BACK)  buttons[1] |= SwitchPro::Buttons1::MINUS;
    if (gp_in.buttons & Gamepad::BUTTON_START) buttons[1] |= SwitchPro::Buttons1::PLUS;
    if (gp_in.buttons & Gamepad::BUTTON_L3)    buttons[1] |= SwitchPro::Buttons1::L3;
    if (gp_in.buttons & Gamepad::BUTTON_R3)    buttons[1] |= SwitchPro::Buttons1::R3;
    if (gp_in.buttons & Gamepad::BUTTON_SYS)   buttons[1] |= SwitchPro::Buttons1::HOME;
    if (gp_in.buttons & Gamepad::BUTTON_MISC)  buttons[1] |= SwitchPro::Buttons1::CAPTURE;

    if (gp_in.dpad & Gamepad::DPAD_UP)    buttons[2] |= SwitchPro::Buttons2::DPAD_UP;
    if (gp_in.dpad & Gamepad::DPAD_DOWN)  buttons[2] |= SwitchPro::Buttons2::DPAD_DOWN;
    if (gp_in.dpad & Gamepad::DPAD_LEFT)  buttons[2] |= SwitchPro::Buttons2::DPAD_LEFT;
    if (gp_in.dpad & Gamepad::DPAD_RIGHT) buttons[2] |= SwitchPro::Buttons2::DPAD_RIGHT;
    if (gp_in.buttons & Gamepad::BUTTON_LB) buttons[2] |= SwitchPro::Buttons2::L;
    if (gp_in.trigger_l)                    buttons[2] |= SwitchPro::Buttons2::ZL;

    std::memcpy(&buffer[1], buttons, sizeof(buttons));

    SwitchProCodec::pack_stick(gp_in.joystick_lx, gp_in.joystick_ly, &buffer[4]);
    SwitchProCodec::pack_stick(gp_in.joystick_rx, gp_in.joystick_ry, &buffer[7]);

    buffer[10] = VIBRATOR_REPORT;
}

void SwitchProDevice::process(const uint8_t idx, Gamepad& gamepad)
{
    //Handle queued host reports until one needs a reply, replies go out one
    //per IN transfer.
    while (out_count_ > 0 && pending_reply_len_ == 0)
    {
        const OutReport& report = out_queue_[out_head_];
        out_head_ = (out_head_ + 1) % OUT_QUEUE_LEN;
        --out_count_;
        handle_out_report(report.data.data(), report.len, gamepad);
    }

    if (tud_suspended())
    {
        tud_remote_wakeup();
    }
    if (!tud_hid_n_ready(idx))
    {
        return;
    }

    const uint32_t now_us = time_us_32();

    if (pending_reply_len_ > 0)
    {
        const uint16_t len = pending_reply_len_;
        pending_reply_len_ = 0;
        //Subcommand replies carry the pad state as of now.
        if (pending_reply_[0] == REPORT_ID_SUBCMD_REPLY)
        {
            pending_reply_[1] = timer_;
            fill_pad_state(gamepad, &pending_reply_[2]);
        }
        tud_hid_n_report(idx, 0, pending_reply_.data(), len);
        last_report_us_ = now_us;
        return;
    }

    if (!full_reports_)
    {
        //Until the host picks a mode, offer the controller info now and then,
        //as 2wiCC does.
        if ((now_us - last_report_us_) >= ANNOUNCE_INTERVAL_US)
        {
            queue_controller_info();
        }
        return;
    }

    if ((now_us - last_report_us_) < FULL_REPORT_INTERVAL_US)
    {
        return;
    }
    last_report_us_ = now_us;

    in_report_.fill(0);
    in_report_[0] = REPORT_ID_FULL;
    in_report_[1] = timer_;
    timer_ = static_cast<uint8_t>(timer_ + 3);

    fill_pad_state(gamepad, &in_report_[2]);

    Gamepad::PadMotion motion = gamepad.get_pad_motion();
    int16_t accel[3] = { 0, 0, 0 };
    int16_t gyro[3] = { 0, 0, 0 };

    if (motion.valid & Gamepad::MOTION_VALID_ACCEL)
    {
        const int16_t accel_mg[3] = { motion.accel_x, motion.accel_y, motion.accel_z };
        SwitchProCodec::convert_accel(accel_mg, imu_calibration(), accel);
    }
    if (motion.valid & Gamepad::MOTION_VALID_GYRO)
    {
        const int16_t gyro_dds[3] = { motion.gyro_x, motion.gyro_y, motion.gyro_z };
        SwitchProCodec::convert_gyro(gyro_dds, imu_calibration(), gyro);
    }

    //The report carries three 5ms samples; the bridge has one, repeat it.
    for (uint8_t frame = 0; frame < 3; ++frame)
    {
        uint8_t* imu = &in_report_[13 + frame * 12];
        std::memcpy(&imu[0], accel, sizeof(accel));
        std::memcpy(&imu[6], gyro, sizeof(gyro));
    }

    tud_hid_n_report(idx, 0, in_report_.data(), REPORT_LEN);
}

void SwitchProDevice::queue_controller_info()
{
    uint8_t reply[REPORT_LEN] = {};
    reply[0] = REPORT_ID_VENDOR_REPLY;
    reply[1] = 0x01;
    reply[3] = 0x03; //Pro Controller
    for (uint8_t i = 0; i < 6; ++i)
    {
        reply[4 + i] = MAC[5 - i];
    }
    queue_reply(reply, REPORT_LEN);
}

void SwitchProDevice::queue_reply(const uint8_t* data, uint16_t len)
{
    if (len > REPORT_LEN)
    {
        len = REPORT_LEN;
    }
    pending_reply_.fill(0);
    std::memcpy(pending_reply_.data(), data, len);
    pending_reply_len_ = REPORT_LEN;
}

uint16_t SwitchProDevice::read_spi(uint32_t address, uint8_t length, uint8_t* out)
{
    for (uint8_t i = 0; i < length; ++i)
    {
        const uint32_t at = address + i;
        uint8_t value = 0xFF;
        if (at >= 0x2000 && at < 0x2000 + sizeof(SPI_0x2000))
        {
            value = SPI_0x2000[at - 0x2000];
        }
        else if (at >= 0x6000 && at < 0x6000 + sizeof(SPI_0x6000))
        {
            value = SPI_0x6000[at - 0x6000];
        }
        out[i] = value;
    }
    return length;
}

void SwitchProDevice::set_rumble(const uint8_t* rumble_data, uint16_t len, Gamepad& gamepad)
{
    if (len < 8)
    {
        return;
    }
    const uint16_t scale = rumble_intensity();

    Gamepad::PadOut gp_out;
    gp_out.rumble_l = static_cast<uint8_t>(SwitchProCodec::decode_rumble(&rumble_data[0]) * scale / 255);
    gp_out.rumble_r = static_cast<uint8_t>(SwitchProCodec::decode_rumble(&rumble_data[4]) * scale / 255);
    gamepad.set_pad_out(gp_out);
}

void SwitchProDevice::handle_subcommand(const uint8_t* buffer, uint16_t bufsize, Gamepad& gamepad)
{
    if (bufsize < 11)
    {
        return;
    }

    set_rumble(&buffer[2], bufsize - 2, gamepad);

    const uint8_t sub_command = buffer[10];
    const uint8_t* args = &buffer[11];
    const uint16_t args_len = static_cast<uint16_t>(bufsize - 11);

    //0x21 reply: id, timer, 11 bytes of pad state (filled when sent), ack at
    //13, subcommand at 14, data from 15.
    uint8_t reply[REPORT_LEN] = {};
    reply[0] = REPORT_ID_SUBCMD_REPLY;
    reply[13] = 0x80; //generic ack
    reply[14] = sub_command;

    switch (sub_command)
    {
        case 0x01: //Bluetooth pairing, nothing to pair over USB
            reply[13] = 0x81;
            reply[15] = (args_len > 0) ? args[0] : 0x00;
            break;
        case 0x02: //request device info
            reply[13] = 0x82;
            reply[15] = 0x03; //firmware version
            reply[16] = 0x48;
            reply[17] = 0x03; //Pro Controller
            reply[18] = 0x02;
            for (uint8_t i = 0; i < 6; ++i)
            {
                reply[19 + i] = MAC[i];
            }
            reply[25] = 0x01;
            reply[26] = 0x01; //colours from SPI
            break;
        case 0x03: //set input report mode
            full_reports_ = (args_len > 0) && (args[0] == SwitchPro::CMD::FULL_REPORT_MODE);
            break;
        case 0x04: //trigger buttons elapsed time, 2wiCC's canned answer
        {
            reply[13] = 0x83;
            const uint8_t elapsed[] = { 0x00, 0xCC, 0x00, 0xEE, 0x00, 0xFF };
            std::memcpy(&reply[15], elapsed, sizeof(elapsed));
            break;
        }
        case 0x10: //SPI flash read
        {
            if (args_len < 5)
            {
                break;
            }
            const uint32_t address = static_cast<uint32_t>(args[0]) |
                                     (static_cast<uint32_t>(args[1]) << 8) |
                                     (static_cast<uint32_t>(args[2]) << 16) |
                                     (static_cast<uint32_t>(args[3]) << 24);
            uint8_t length = args[4];
            if (length > REPORT_LEN - 20)
            {
                length = REPORT_LEN - 20;
            }
            reply[13] = 0x90;
            std::memcpy(&reply[15], args, 5);
            read_spi(address, length, &reply[20]);
            break;
        }
        default: //0x00, 0x08, 0x11, 0x12, 0x30, 0x38, 0x40, 0x41, 0x48 and friends just ack
            break;
    }

    queue_reply(reply, REPORT_LEN);
}

void SwitchProDevice::handle_out_report(const uint8_t* buffer, uint16_t bufsize, Gamepad& gamepad)
{
    if (bufsize < 1)
    {
        return;
    }

    switch (buffer[0])
    {
        case SwitchPro::CMD::HID: //0x80, vendor handshake
        {
            if (bufsize < 2)
            {
                return;
            }
            uint8_t reply[REPORT_LEN] = {};
            reply[0] = REPORT_ID_VENDOR_REPLY;
            reply[1] = buffer[1];

            switch (buffer[1])
            {
                case 0x01: //request controller info
                    queue_controller_info();
                    break;
                case SwitchPro::CMD::HANDSHAKE:       //0x02
                case 0x03:                            //baud rate
                    queue_reply(reply, REPORT_LEN);
                    break;
                case SwitchPro::CMD::DISABLE_TIMEOUT: //0x04, host takes over
                    full_reports_ = true;
                    reply[1] = 0x05; //2wiCC answers 0x04 and 0x05 with each other's id
                    queue_reply(reply, REPORT_LEN);
                    break;
                case 0x05: //enable timeout, stop streaming
                    full_reports_ = false;
                    reply[1] = 0x04;
                    queue_reply(reply, REPORT_LEN);
                    break;
                default:
                    queue_reply(reply, REPORT_LEN);
                    break;
            }
            break;
        }
        case SwitchPro::CMD::AND_RUMBLE: //0x01, rumble plus subcommand
            handle_subcommand(buffer, bufsize, gamepad);
            break;
        case SwitchPro::CMD::RUMBLE_ONLY: //0x10
            if (bufsize >= 10)
            {
                set_rumble(&buffer[2], bufsize - 2, gamepad);
            }
            break;
        default:
            break;
    }
}

uint16_t SwitchProDevice::get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
    if (report_type != HID_REPORT_TYPE_INPUT || reqlen < 49)
    {
        return 0;
    }
    std::memcpy(buffer, in_report_.data(), 49);
    return 49;
}

void SwitchProDevice::set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
    if (!buffer || bufsize == 0 || report_type == HID_REPORT_TYPE_INPUT)
    {
        return;
    }

    if (out_count_ == OUT_QUEUE_LEN)
    {
        return; //host outran us; it resends what it doesn't get an answer to
    }
    OutReport& report = out_queue_[(out_head_ + out_count_) % OUT_QUEUE_LEN];
    ++out_count_;

    //Reports off the OUT endpoint carry their id in the first byte, reports
    //that arrive over the control pipe do not.
    report.data.fill(0);
    uint16_t offset = 0;
    if (report_id != 0)
    {
        report.data[0] = report_id;
        offset = 1;
    }
    if (bufsize > REPORT_LEN - offset)
    {
        bufsize = REPORT_LEN - offset;
    }
    std::memcpy(&report.data[offset], buffer, bufsize);
    report.len = static_cast<uint16_t>(bufsize + offset);
}

bool SwitchProDevice::vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request)
{
    return false;
}

const uint16_t* SwitchProDevice::get_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    return get_string_descriptor(SwitchPro::STRING_DESCRIPTORS, index);
}

const uint8_t* SwitchProDevice::get_descriptor_device_cb()
{
    return SwitchPro::DEVICE_DESCRIPTORS;
}

const uint8_t* SwitchProDevice::get_hid_descriptor_report_cb(uint8_t itf)
{
    return SwitchPro::REPORT_DESCRIPTORS;
}

const uint8_t* SwitchProDevice::get_descriptor_configuration_cb(uint8_t index)
{
    return SwitchPro::CONFIGURATION_DESCRIPTORS;
}

const uint8_t* SwitchProDevice::get_descriptor_device_qualifier_cb()
{
    return nullptr;
}
