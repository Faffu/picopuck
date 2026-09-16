#ifndef _SWITCH_PRO_CODEC_H_
#define _SWITCH_PRO_CODEC_H_

#include <cstddef>
#include <cstdint>

/*  Pure conversions used by the Switch Pro device driver: stick packing, IMU
    unit conversion and rumble decoding. Kept free of pico/tinyusb includes so
    it can be built and tested on the host.  */

namespace SwitchProCodec {

static constexpr uint16_t STICK_MID = 2048;
static constexpr uint16_t STICK_MAX = 4095;

//Packs a centered stick pair into the 12+12 bit form of the 0x30 report.
void pack_stick(int16_t x, int16_t y, uint8_t out[3]);

//Per axis remap, applied before scaling: out[i] = sign[i] * in[src[i]].
struct AxisMap
{
    uint8_t src[3]{0, 1, 2};
    int8_t sign[3]{1, 1, 1};
};

/*  Gamepad::PadMotion units (milli-g, 0.1 deg/s) to Switch sensor counts.
    Switch Pro reports accel at 4096 counts per g and gyro at 0.07 deg/s per
    count; num/den keep both adjustable for calibration.  */
struct ImuCalibration
{
    AxisMap accel_map{};
    AxisMap gyro_map{};
    int32_t accel_num{4096};
    int32_t accel_den{1000};
    int32_t gyro_num{10};
    int32_t gyro_den{7};
    int16_t accel_offset[3]{0, 0, 0};
    int16_t gyro_offset[3]{0, 0, 0};
};

//accel_mg / gyro_dds hold x,y,z in the PadMotion units, out holds Switch counts.
void convert_accel(const int16_t accel_mg[3], const ImuCalibration& cal, int16_t out[3]);
void convert_gyro(const int16_t gyro_dds[3], const ImuCalibration& cal, int16_t out[3]);

/*  Decodes one side of a Switch rumble packet (4 bytes) to a 0-255 amplitude.
    ponytail: amplitude only, frequency is dropped; add the frequency tables if
    a game needs the difference between a buzz and a thump.  */
uint8_t decode_rumble(const uint8_t data[4]);

} // namespace SwitchProCodec

#endif // _SWITCH_PRO_CODEC_H_
