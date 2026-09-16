#ifndef _SWITCH_PRO_DEVICE_H_
#define _SWITCH_PRO_DEVICE_H_

#include <array>
#include <cstdint>

#include "USBDevice/DeviceDriver/DeviceDriver.h"
#include "USBDevice/DeviceDriver/SwitchPro/SwitchProCodec.h"
#include "Descriptors/SwitchPro.h"

/*  Nintendo Switch Pro Controller, wired USB. Target is the Switch 2 dock with
    wired Pro Controller communication enabled, but the same emulation is what
    a Switch 1 and Steam expect.

    Separate from the HORI wired pad (DeviceDriverType::SWITCH): the Pro adds
    the vendor handshake, the subcommand protocol, motion and rumble.  */

class SwitchProDevice : public DeviceDriver
{
public:
    void initialize() override;
    void process(const uint8_t idx, Gamepad& gamepad) override;
    uint16_t get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) override;
    void set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) override;
    bool vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request) override;
    const uint16_t* get_descriptor_string_cb(uint8_t index, uint16_t langid) override;
    const uint8_t* get_descriptor_device_cb() override;
    const uint8_t* get_hid_descriptor_report_cb(uint8_t itf) override;
    const uint8_t* get_descriptor_configuration_cb(uint8_t index) override;
    const uint8_t* get_descriptor_device_qualifier_cb() override;
    bool echo_set_report() const override { return false; }

    //Motion calibration, kept public so it stays tunable from one place.
    static SwitchProCodec::ImuCalibration& imu_calibration();
    //Rumble strength, 0-255 scales the amplitude reported to the input side.
    static uint8_t& rumble_intensity();

private:
    static constexpr size_t REPORT_LEN = 64;
    static constexpr uint8_t REPORT_ID_FULL = 0x30;
    static constexpr uint8_t REPORT_ID_SUBCMD_REPLY = 0x21;
    static constexpr uint8_t REPORT_ID_VENDOR_REPLY = 0x81;
    static constexpr uint32_t FULL_REPORT_INTERVAL_US = 8000;

    //Reported as the controller's Bluetooth address, arbitrary but stable.
    static constexpr std::array<uint8_t, 6> MAC = { 0x7C, 0xBB, 0x8A, 0x4D, 0x1E, 0x2F };

    std::array<uint8_t, REPORT_LEN> in_report_{};
    std::array<uint8_t, REPORT_LEN> pending_reply_{};
    uint16_t pending_reply_len_{0};

    //Output reports as received, the host can send several between two
    //process() calls (rumble plus a subcommand). Same core, no locking.
    static constexpr size_t OUT_QUEUE_LEN = 8;
    struct OutReport
    {
        std::array<uint8_t, REPORT_LEN> data;
        uint16_t len;
    };
    std::array<OutReport, OUT_QUEUE_LEN> out_queue_{};
    size_t out_head_{0};
    size_t out_count_{0};

    uint8_t timer_{0};
    uint32_t last_report_us_{0};
    bool full_reports_{false};

    void fill_pad_state(Gamepad& gamepad, uint8_t* buffer);
    void handle_out_report(const uint8_t* buffer, uint16_t bufsize, Gamepad& gamepad);
    void handle_subcommand(const uint8_t* buffer, uint16_t bufsize, Gamepad& gamepad);
    void queue_reply(const uint8_t* data, uint16_t len);
    void queue_controller_info();
    void set_rumble(const uint8_t* rumble_data, uint16_t len, Gamepad& gamepad);
    static uint16_t read_spi(uint32_t address, uint8_t length, uint8_t* out);
};

#endif // _SWITCH_PRO_DEVICE_H_
