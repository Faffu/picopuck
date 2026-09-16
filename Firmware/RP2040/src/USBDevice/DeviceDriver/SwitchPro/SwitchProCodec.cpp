#include "USBDevice/DeviceDriver/SwitchPro/SwitchProCodec.h"

namespace SwitchProCodec {

namespace {

uint16_t to_stick_axis(int16_t value)
{
    //-32768..32767 to 0..4095, centered on STICK_MID.
    int32_t scaled = (static_cast<int32_t>(value) * STICK_MID) / 32768 + STICK_MID;
    if (scaled < 0) return 0;
    if (scaled > STICK_MAX) return STICK_MAX;
    return static_cast<uint16_t>(scaled);
}

int16_t clamp_i16(int32_t value)
{
    if (value > INT16_MAX) return INT16_MAX;
    if (value < INT16_MIN) return INT16_MIN;
    return static_cast<int16_t>(value);
}

void convert(const int16_t in[3], const AxisMap& map, int32_t num, int32_t den,
             const int16_t offset[3], int16_t out[3])
{
    for (uint8_t i = 0; i < 3; ++i)
    {
        const uint8_t src = (map.src[i] < 3) ? map.src[i] : i;
        int32_t value = static_cast<int32_t>(in[src]) * map.sign[i];
        value = (den != 0) ? (value * num / den) : 0;
        out[i] = clamp_i16(value + offset[i]);
    }
}

} // namespace

void pack_stick(int16_t x, int16_t y, uint8_t out[3])
{
    const uint16_t px = to_stick_axis(x);
    //Report grows upwards, gamepad state grows downwards.
    const uint16_t py = to_stick_axis((y == INT16_MIN) ? INT16_MAX : static_cast<int16_t>(-y));

    out[0] = static_cast<uint8_t>(px & 0xFF);
    out[1] = static_cast<uint8_t>(((px >> 8) & 0x0F) | ((py & 0x0F) << 4));
    out[2] = static_cast<uint8_t>((py >> 4) & 0xFF);
}

void convert_accel(const int16_t accel_mg[3], const ImuCalibration& cal, int16_t out[3])
{
    convert(accel_mg, cal.accel_map, cal.accel_num, cal.accel_den, cal.accel_offset, out);
}

void convert_gyro(const int16_t gyro_dds[3], const ImuCalibration& cal, int16_t out[3])
{
    convert(gyro_dds, cal.gyro_map, cal.gyro_num, cal.gyro_den, cal.gyro_offset, out);
}

uint8_t decode_rumble(const uint8_t data[4])
{
    //High band amplitude is a 7 bit index, low band is offset by 0x40.
    const uint16_t high = static_cast<uint16_t>((data[1] & 0xFE) >> 1);
    const uint8_t low_raw = static_cast<uint8_t>(data[3] & 0x7F);
    const uint16_t low = (low_raw > 0x40) ? static_cast<uint16_t>(low_raw - 0x40) : 0;

    uint16_t amplitude = (high > low) ? high : low;
    amplitude *= 2;
    return (amplitude > 255) ? 255 : static_cast<uint8_t>(amplitude);
}

} // namespace SwitchProCodec
