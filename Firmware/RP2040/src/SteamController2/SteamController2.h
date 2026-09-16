#ifndef _STEAM_CONTROLLER_2_H_
#define _STEAM_CONTROLLER_2_H_

#include <cstddef>
#include <cstdint>

/*  Decoder for the Steam Controller 2 (28de:1303) BLE state report.

    The controller sends HID over GATT reports. Report 0x45 carries the whole
    state at about 133 Hz; 0x40 (mouse), 0x41 (keyboard) and 0x43 (status) are
    ignored. Layout measured from a capture, little endian, offsets include
    the report id:

       0      report id 0x45
       1      sequence, +1 per report
       2..5   buttons, BTN_* bits
       6..9   trigger l, r: 0..32767
      10..17  stick lx, ly, rx, ry: int16, y up positive
      18..23  left pad x, y (int16), pressure (uint16)
      24..29  right pad x, y (int16), pressure (uint16)
      30..33  sensor timestamp, microseconds
      34..39  accel x, y, z: +/-2 g full scale, z = +1 g lying face up
      40..45  gyro x, y, z: +/-2000 deg/s full scale, same frame as accel

    Measured from a capture, then checked against SDL's Valve written driver
    (src/joystick/hidapi/SDL_hidapi_steam_triton.c, "Triton"), which also
    supplied the Steam, quick access, view and menu bits and the rumble output
    report.  */

namespace SteamController2 {

static constexpr uint8_t REPORT_ID  = 0x45;
static constexpr size_t  REPORT_LEN = 46;

static constexpr uint32_t BTN_A          = 0x00000001;
static constexpr uint32_t BTN_B          = 0x00000002;
static constexpr uint32_t BTN_X          = 0x00000004;
static constexpr uint32_t BTN_Y          = 0x00000008;
static constexpr uint32_t BTN_QUICK_ACCESS = 0x00000010;
static constexpr uint32_t BTN_THUMB_R    = 0x00000020;
//View and menu as tested on the controller; SDL names these two bits the
//other way round.
static constexpr uint32_t BTN_MENU       = 0x00000040;
static constexpr uint32_t BTN_R4         = 0x00000080;
static constexpr uint32_t BTN_R5         = 0x00000100;
static constexpr uint32_t BTN_SHOULDER_R = 0x00000200;
static constexpr uint32_t BTN_DPAD_DOWN  = 0x00000400;
static constexpr uint32_t BTN_DPAD_RIGHT = 0x00000800;
static constexpr uint32_t BTN_DPAD_LEFT  = 0x00001000;
static constexpr uint32_t BTN_DPAD_UP    = 0x00002000;
static constexpr uint32_t BTN_VIEW       = 0x00004000;
static constexpr uint32_t BTN_THUMB_L    = 0x00008000;
static constexpr uint32_t BTN_STEAM      = 0x00010000;
static constexpr uint32_t BTN_L4         = 0x00020000;
static constexpr uint32_t BTN_L5         = 0x00040000;
static constexpr uint32_t BTN_SHOULDER_L = 0x00080000;
static constexpr uint32_t BTN_STICK_R_TOUCH = 0x00100000;
static constexpr uint32_t BTN_PAD_R_TOUCH   = 0x00200000;
static constexpr uint32_t BTN_PAD_R_CLICK   = 0x00400000;
static constexpr uint32_t BTN_TRIGGER_R     = 0x00800000; //full pull click
static constexpr uint32_t BTN_STICK_L_TOUCH = 0x01000000;
static constexpr uint32_t BTN_PAD_L_TOUCH   = 0x02000000;
static constexpr uint32_t BTN_PAD_L_CLICK   = 0x04000000;
static constexpr uint32_t BTN_TRIGGER_L     = 0x08000000; //full pull click
static constexpr uint32_t BTN_GRIP_R_TOUCH  = 0x10000000;
static constexpr uint32_t BTN_GRIP_L_TOUCH  = 0x20000000;

//Back paddle bits of State::back_buttons.
static constexpr uint8_t BACK_L4 = 0x01;
static constexpr uint8_t BACK_R4 = 0x02;
static constexpr uint8_t BACK_L5 = 0x04;
static constexpr uint8_t BACK_R5 = 0x08;

struct State
{
    uint32_t buttons;       //BTN_* bits
    uint8_t  trigger_l;     //0..255
    uint8_t  trigger_r;
    int16_t  stick_lx, stick_ly;  //y grows downwards, like Gamepad
    int16_t  stick_rx, stick_ry;
    int16_t  pad_lx, pad_ly;
    int16_t  pad_rx, pad_ry;
    int16_t  accel_x, accel_y, accel_z;  //raw sensor counts
    int16_t  gyro_x, gyro_y, gyro_z;     //raw sensor counts
    uint8_t  back_buttons;  //BACK_* bits
};

//Returns false and leaves state untouched unless the report is a full 0x45
//state report.
bool decode(const uint8_t* report, size_t len, State& state);

//Haptic rumble output report. The controller stops the motors about 50 ms
//after the last one, so it has to be resent while rumble is on.
static constexpr uint8_t RUMBLE_REPORT_ID = 0x80;
static constexpr size_t  RUMBLE_REPORT_LEN = 9; //without the report id
static constexpr uint32_t RUMBLE_RESEND_MS = 40;

//Fills the rumble report payload: left motor from the low frequency (strong)
//amplitude, right motor from the high frequency (weak) one, 0..255 each.
void encode_rumble(uint8_t strong, uint8_t weak, uint8_t out[RUMBLE_REPORT_LEN]);

//Sensor counts to the units Gamepad::PadMotion uses (milli-g, 0.1 deg/s):
//  out = counts * num / den, defaults are the controller's full scale ranges.
struct MotionScale
{
    int32_t accel_num{1000};
    int32_t accel_den{16384};
    int32_t gyro_num{20000};
    int32_t gyro_den{32768};
};

int16_t scale_accel(int16_t counts, const MotionScale& scale);
int16_t scale_gyro(int16_t counts, const MotionScale& scale);

} // namespace SteamController2

#endif // _STEAM_CONTROLLER_2_H_
