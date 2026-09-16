// Host tests for the parts of the bridge that are pure logic.
// Build and run: Tools/pico-bridge/run_tests.sh

#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

#include "SteamController2/ReportCapture.h"
#include "SteamController2/SteamController2.h"
#include "USBDevice/DeviceDriver/SwitchPro/SwitchProCodec.h"

namespace {

//Reports copied from a Steam Controller 2 capture (capture mode, 28de:1303).
std::vector<uint8_t> hex_report(const char* hex)
{
    std::vector<uint8_t> report;
    unsigned byte = 0;
    int consumed = 0;
    while (std::sscanf(hex, "%2x%n", &byte, &consumed) == 1)
    {
        report.push_back(static_cast<uint8_t>(byte));
        hex += consumed;
    }
    return report;
}

//A held, lying face up.
const char* SC2_A = "45 ca 01 00 00 00 00 00 00 00 83 01 ce 01 16 fe 7b 02 00 00 00 00 00 00 00 00 00 00 00 00 c3 ba 82 05 80 fe 1b ff ca 42 0d 00 f3 ff fe ff";
//Left trigger pulled through the click.
const char* SC2_TRIGGER_L = "45 b4 00 00 00 08 ff 7f 00 00 56 00 3c 05 55 ff c4 02 00 00 00 00 00 00 00 00 00 00 00 00 43 be af 06 66 f9 41 fe 5c 3f 15 00 1e 00 25 00";
//Left stick fully up.
const char* SC2_STICK_L_UP = "45 b8 00 00 00 31 00 00 00 00 bd 07 ff 7f 3c ff 93 02 00 00 00 00 00 00 00 00 00 00 00 00 48 80 1e 07 03 fd 55 1c 39 35 3b 01 be ff 2a 00";
//Right pad clicked, off centre.
const char* SC2_PAD_R_CLICK = "45 61 00 00 60 30 00 00 00 00 9b 02 87 02 5a 00 f4 00 00 00 00 00 00 00 60 03 6a 2a ff 11 31 1b 60 08 9a fd ac ff e3 3f e9 ff 47 ff b0 ff";

void test_decode_captured_reports()
{
    SteamController2::State state{};

    auto a = hex_report(SC2_A);
    assert(a.size() == SteamController2::REPORT_LEN);
    assert(SteamController2::decode(a.data(), a.size(), state));
    assert(state.buttons == SteamController2::BTN_A);
    assert(state.trigger_l == 0 && state.trigger_r == 0);
    assert(state.stick_lx == 0x0183);  //resting offset, left to the deadzone
    assert(state.accel_z == 0x42ca);   //about +1 g lying face up
    assert(state.back_buttons == 0);

    auto trigger = hex_report(SC2_TRIGGER_L);
    assert(SteamController2::decode(trigger.data(), trigger.size(), state));
    assert(state.buttons & SteamController2::BTN_TRIGGER_L);
    assert(state.trigger_l == 0xFF);

    auto stick = hex_report(SC2_STICK_L_UP);
    assert(SteamController2::decode(stick.data(), stick.size(), state));
    assert(state.stick_ly == -32767); //up is negative in gamepad terms
    assert(state.buttons & SteamController2::BTN_STICK_L_TOUCH);

    auto pad = hex_report(SC2_PAD_R_CLICK);
    assert(SteamController2::decode(pad.data(), pad.size(), state));
    assert(state.buttons & SteamController2::BTN_PAD_R_CLICK);
    assert(state.buttons & SteamController2::BTN_PAD_R_TOUCH);
    assert(state.pad_rx == 0x0360);
    assert(state.pad_ry == -0x2a6a);

    SteamController2::MotionScale scale;
    assert(SteamController2::scale_accel(16384, scale) == 1000);   //milli-g
    assert(SteamController2::scale_gyro(16384, scale) == 10000);   //0.1 deg/s, 1000 deg/s
    assert(SteamController2::scale_gyro(0, scale) == 0);
}

void test_encode_rumble()
{
    uint8_t out[SteamController2::RUMBLE_REPORT_LEN];
    SteamController2::encode_rumble(0xFF, 0x80, out);
    const uint8_t expected[] = {0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x80, 0x80, 0x00};
    assert(std::memcmp(out, expected, sizeof(expected)) == 0);

    SteamController2::encode_rumble(0, 0, out);
    for (uint8_t byte : out)
    {
        assert(byte == 0);
    }
}

void test_decode_back_buttons()
{
    auto report = hex_report(SC2_A);
    report[2] = 0x80; //R4
    report[4] = 0x04; //L5
    SteamController2::State state{};
    assert(SteamController2::decode(report.data(), report.size(), state));
    assert(state.back_buttons == (SteamController2::BACK_R4 | SteamController2::BACK_L5));
}

void test_decode_rejects_other_reports()
{
    SteamController2::State state{};
    state.buttons = 0xDEAD;
    const SteamController2::State original = state;

    //Status and lizard mode mouse reports from the same capture.
    auto status = hex_report("43 01 61 1a 10 40 10 00 00 00 00 00 00 42 73");
    assert(!SteamController2::decode(status.data(), status.size(), state));
    auto mouse = hex_report("40 02 00 00 00 00");
    assert(!SteamController2::decode(mouse.data(), mouse.size(), state));

    //Truncated state report.
    auto truncated = hex_report(SC2_A);
    truncated.pop_back();
    assert(!SteamController2::decode(truncated.data(), truncated.size(), state));
    assert(!SteamController2::decode(nullptr, SteamController2::REPORT_LEN, state));

    assert(std::memcmp(&state, &original, sizeof(state)) == 0);
}

void test_pack_stick()
{
    uint8_t out[3] = {};

    SwitchProCodec::pack_stick(0, 0, out);
    uint16_t x = out[0] | ((out[1] & 0x0F) << 8);
    uint16_t y = (out[1] >> 4) | (out[2] << 4);
    assert(x == SwitchProCodec::STICK_MID);
    assert(y == SwitchProCodec::STICK_MID);

    SwitchProCodec::pack_stick(32767, 32767, out);
    x = out[0] | ((out[1] & 0x0F) << 8);
    y = (out[1] >> 4) | (out[2] << 4);
    assert(x > SwitchProCodec::STICK_MID + 2000); //right
    assert(y < SwitchProCodec::STICK_MID - 2000); //stick down in report terms

    SwitchProCodec::pack_stick(-32768, -32768, out);
    x = out[0] | ((out[1] & 0x0F) << 8);
    assert(x == 0);
}

void test_imu_conversion()
{
    SwitchProCodec::ImuCalibration cal;
    int16_t out[3] = {};

    const int16_t one_g[3] = {1000, 0, 0}; //1 g on x
    SwitchProCodec::convert_accel(one_g, cal, out);
    assert(out[0] == 4096);
    assert(out[1] == 0);

    const int16_t gyro[3] = {0, 700, 0}; //70 deg/s on y
    SwitchProCodec::convert_gyro(gyro, cal, out);
    assert(out[1] == 1000); //70 / 0.07

    //Orientation: swap x and z, invert z.
    cal.gyro_map.src[0] = 2;
    cal.gyro_map.src[2] = 0;
    cal.gyro_map.sign[2] = -1;
    const int16_t gyro_xyz[3] = {700, 0, 350};
    SwitchProCodec::convert_gyro(gyro_xyz, cal, out);
    assert(out[0] == 500);   //z into x
    assert(out[2] == -1000); //x into z, inverted

    //Offset knob still applies.
    cal.gyro_offset[1] = 25;
    SwitchProCodec::convert_gyro(gyro, cal, out);
    assert(out[1] == 1025);
}

void test_rumble_decode()
{
    const uint8_t neutral[4] = {0x00, 0x01, 0x40, 0x40};
    assert(SwitchProCodec::decode_rumble(neutral) == 0);

    const uint8_t strong_high[4] = {0x00, 0xC8, 0x40, 0x40};
    assert(SwitchProCodec::decode_rumble(strong_high) > 100);

    const uint8_t strong_low[4] = {0x00, 0x01, 0x40, 0x72};
    assert(SwitchProCodec::decode_rumble(strong_low) > 0);

    const uint8_t full[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    assert(SwitchProCodec::decode_rumble(full) >= 250);
}

void test_report_capture()
{
    ReportCapture::reset();

    //Reports are ignored until the consumer enables them, text is not.
    const uint8_t early[] = {0x45};
    ReportCapture::push(early, sizeof(early), 1);
    ReportCapture::Entry skipped{};
    assert(!ReportCapture::pop(skipped));
    ReportCapture::set_reports_enabled(true);

    const uint8_t first[] = {0x03, 0xc0, 0x24, 0x00};
    const uint8_t second[] = {0x03, 0xc0, 0x14};
    ReportCapture::push(first, sizeof(first), 1234);
    ReportCapture::push(second, sizeof(second), 5678);

    ReportCapture::Entry entry{};
    assert(ReportCapture::pop(entry));
    assert(entry.timestamp_us == 1234);
    assert(entry.len == sizeof(first));
    assert(std::memcmp(entry.data, first, sizeof(first)) == 0);

    assert(ReportCapture::pop(entry));
    assert(entry.len == sizeof(second));
    assert(!ReportCapture::pop(entry)); //ring is empty again

    //Oversized reports are stored truncated and counted.
    uint8_t oversized[ReportCapture::MAX_REPORT_LEN + 8];
    std::memset(oversized, 0xAB, sizeof(oversized));
    ReportCapture::push(oversized, sizeof(oversized), 1);
    assert(ReportCapture::pop(entry));
    assert(entry.len == ReportCapture::MAX_REPORT_LEN);
    assert(ReportCapture::truncated() == 1);

    //A full ring drops new reports rather than overwriting unread ones.
    ReportCapture::reset();
    ReportCapture::set_reports_enabled(true);
    for (size_t i = 0; i < ReportCapture::CAPACITY + 5; ++i)
    {
        const uint8_t byte = static_cast<uint8_t>(i);
        ReportCapture::push(&byte, 1, static_cast<uint32_t>(i));
    }
    assert(ReportCapture::dropped() == 5);
    assert(ReportCapture::pop(entry));
    assert(entry.timestamp_us == 0); //oldest is still there

    size_t remaining = 1;
    while (ReportCapture::pop(entry))
    {
        ++remaining;
    }
    assert(remaining == ReportCapture::CAPACITY);

    //Empty reports are ignored.
    assert(!ReportCapture::pop(entry));
    ReportCapture::push(nullptr, 4, 0);
    ReportCapture::push(first, 0, 0);
    assert(!ReportCapture::pop(entry));

    //Text is chunked in order and marked as text.
    ReportCapture::reset();
    char text[ReportCapture::MAX_REPORT_LEN + 3];
    std::memset(text, 'x', ReportCapture::MAX_REPORT_LEN);
    std::memcpy(&text[ReportCapture::MAX_REPORT_LEN], "abc", 3);
    ReportCapture::push_text(text, sizeof(text));
    assert(ReportCapture::pop(entry));
    assert(entry.kind == ReportCapture::KIND_TEXT && entry.len == ReportCapture::MAX_REPORT_LEN);
    assert(ReportCapture::pop(entry));
    assert(entry.len == 3 && std::memcmp(entry.data, "abc", 3) == 0);
    assert(!ReportCapture::pop(entry));
    assert(ReportCapture::truncated() == 0);
}

} // namespace

int main()
{
    test_decode_captured_reports();
    test_decode_back_buttons();
    test_encode_rumble();
    test_decode_rejects_other_reports();
    test_pack_stick();
    test_imu_conversion();
    test_rumble_decode();
    test_report_capture();
    std::printf("all bridge tests passed\n");
    return 0;
}
