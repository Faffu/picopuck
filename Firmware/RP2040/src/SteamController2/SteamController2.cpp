#include "SteamController2/SteamController2.h"

namespace SteamController2 {

namespace {

inline int16_t read_i16(const uint8_t* p)
{
    return static_cast<int16_t>(static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8));
}

inline int16_t invert_axis(int16_t value)
{
    //Reports have y growing upwards, the gamepad state grows downwards.
    return (value == INT16_MIN) ? INT16_MAX : static_cast<int16_t>(-value);
}

inline uint8_t trigger(const uint8_t* p)
{
    const int16_t value = read_i16(p);
    return (value <= 0) ? 0 : static_cast<uint8_t>(value >> 7);
}

int32_t scale(int32_t counts, int32_t num, int32_t den)
{
    if (den == 0)
    {
        return 0;
    }
    int32_t value = counts * num / den;
    if (value > INT16_MAX) return INT16_MAX;
    if (value < INT16_MIN) return INT16_MIN;
    return value;
}

} // namespace

bool decode(const uint8_t* report, size_t len, State& state)
{
    if (!report || len < REPORT_LEN || report[0] != REPORT_ID)
    {
        return false;
    }

    State decoded{};
    decoded.buttons = static_cast<uint32_t>(report[2]) |
                      (static_cast<uint32_t>(report[3]) << 8) |
                      (static_cast<uint32_t>(report[4]) << 16) |
                      (static_cast<uint32_t>(report[5]) << 24);
    decoded.trigger_l = trigger(&report[6]);
    decoded.trigger_r = trigger(&report[8]);
    decoded.stick_lx = read_i16(&report[10]);
    decoded.stick_ly = invert_axis(read_i16(&report[12]));
    decoded.stick_rx = read_i16(&report[14]);
    decoded.stick_ry = invert_axis(read_i16(&report[16]));
    decoded.pad_lx = read_i16(&report[18]);
    decoded.pad_ly = invert_axis(read_i16(&report[20]));
    decoded.pad_rx = read_i16(&report[24]);
    decoded.pad_ry = invert_axis(read_i16(&report[26]));
    decoded.accel_x = read_i16(&report[34]);
    decoded.accel_y = read_i16(&report[36]);
    decoded.accel_z = read_i16(&report[38]);
    decoded.gyro_x = read_i16(&report[40]);
    decoded.gyro_y = read_i16(&report[42]);
    decoded.gyro_z = read_i16(&report[44]);

    const uint32_t b = decoded.buttons;
    decoded.back_buttons = ((b & BTN_L4) ? BACK_L4 : 0) | ((b & BTN_R4) ? BACK_R4 : 0) |
                           ((b & BTN_L5) ? BACK_L5 : 0) | ((b & BTN_R5) ? BACK_R5 : 0);

    state = decoded;
    return true;
}

void encode_rumble(uint8_t strong, uint8_t weak, uint8_t out[RUMBLE_REPORT_LEN])
{
    //type u8, intensity u16, then left and right as speed u16 + gain i8.
    const uint16_t left = static_cast<uint16_t>(strong * 257);
    const uint16_t right = static_cast<uint16_t>(weak * 257);
    out[0] = 0;
    out[1] = 0;
    out[2] = 0;
    out[3] = static_cast<uint8_t>(left);
    out[4] = static_cast<uint8_t>(left >> 8);
    out[5] = 0;
    out[6] = static_cast<uint8_t>(right);
    out[7] = static_cast<uint8_t>(right >> 8);
    out[8] = 0;
}

int16_t scale_accel(int16_t counts, const MotionScale& motion_scale)
{
    return static_cast<int16_t>(scale(counts, motion_scale.accel_num, motion_scale.accel_den));
}

int16_t scale_gyro(int16_t counts, const MotionScale& motion_scale)
{
    return static_cast<int16_t>(scale(counts, motion_scale.gyro_num, motion_scale.gyro_den));
}

} // namespace SteamController2
