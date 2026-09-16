#include <cstdio>
#include <cstring>

#include "class/cdc/cdc_device.h"
#include "bsp/board_api.h"

#include "Descriptors/CDCDev.h"
#include "SteamController2/ReportCapture.h"
#include "USBDevice/DeviceDriver/Capture/Capture.h"

namespace {

//"<seconds>.<microseconds> <len> <hex bytes>\n", one line per report.
size_t format_entry(const ReportCapture::Entry& entry, char* out, size_t out_len)
{
    int written = std::snprintf(out, out_len, "%lu.%06lu %2u ",
                                static_cast<unsigned long>(entry.timestamp_us / 1000000u),
                                static_cast<unsigned long>(entry.timestamp_us % 1000000u),
                                static_cast<unsigned>(entry.len));
    if (written < 0)
    {
        return 0;
    }

    size_t len = static_cast<size_t>(written);
    for (uint8_t i = 0; i < entry.len && len + 3 < out_len; ++i)
    {
        written = std::snprintf(&out[len], out_len - len, "%02x ", entry.data[i]);
        if (written < 0)
        {
            return len;
        }
        len += static_cast<size_t>(written);
    }

    if (len + 1 < out_len)
    {
        out[len++] = '\n';
    }
    return len;
}

void write_line(const char* text, size_t len)
{
    if (tud_cdc_write_available() < len)
    {
        tud_cdc_write_flush();
        if (tud_cdc_write_available() < len)
        {
            return; //terminal isn't draining, drop this line rather than block
        }
    }
    tud_cdc_write(text, static_cast<uint32_t>(len));
    tud_cdc_write_flush();
}

} // namespace

void CaptureDevice::initialize()
{
    class_driver_ =
    {
        .name = TUD_DRV_NAME("CAPTURE"),
        .init = cdcd_init,
        .deinit = cdcd_deinit,
        .reset = cdcd_reset,
        .open = cdcd_open,
        .control_xfer_cb = cdcd_control_xfer_cb,
        .xfer_cb = cdcd_xfer_cb,
        .sof = NULL
    };

    ReportCapture::reset();
}

void CaptureDevice::process(const uint8_t idx, Gamepad& gamepad)
{
    const bool connected = tud_cdc_connected();
    ReportCapture::set_reports_enabled(connected);
    if (!connected)
    {
        banner_sent_ = false;
        return;
    }

    if (!banner_sent_)
    {
        banner_sent_ = true;
        static const char banner[] =
            "OGX-Mini capture mode\r\n"
            "timestamp length report-bytes, other lines are Bluepad32 log\r\n";
        write_line(banner, sizeof(banner) - 1);
    }

    char line[3 * ReportCapture::MAX_REPORT_LEN + 32];

    //Bounded per call so the USB task keeps running.
    for (uint8_t i = 0; i < 8; ++i)
    {
        ReportCapture::Entry entry;
        if (!ReportCapture::pop(entry))
        {
            break;
        }
        if (entry.kind == ReportCapture::KIND_TEXT)
        {
            write_line(reinterpret_cast<const char*>(entry.data), entry.len);
            continue;
        }
        const size_t len = format_entry(entry, line, sizeof(line));
        if (len > 0)
        {
            write_line(line, len);
        }
    }

    const uint32_t dropped = ReportCapture::dropped();
    if (dropped != reported_drops_)
    {
        reported_drops_ = dropped;
        const int len = std::snprintf(line, sizeof(line), "# dropped %lu, truncated %lu\n",
                                      static_cast<unsigned long>(dropped),
                                      static_cast<unsigned long>(ReportCapture::truncated()));
        if (len > 0)
        {
            write_line(line, static_cast<size_t>(len));
        }
    }
}

uint16_t CaptureDevice::get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
    return reqlen;
}

void CaptureDevice::set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
}

bool CaptureDevice::vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request)
{
    return false;
}

const uint16_t* CaptureDevice::get_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    static uint16_t desc_str[33];
    size_t char_count = 0;

    switch (index)
    {
        case 0:
            std::memcpy(&desc_str[1], CDCDesc::DESC_STRING[0], 2);
            char_count = 1;
            break;

        case 3:
            char_count = board_usb_get_serial(&desc_str[1], 32);
            break;

        default:
        {
            if (index >= sizeof(CDCDesc::DESC_STRING) / sizeof(CDCDesc::DESC_STRING[0]))
            {
                return nullptr;
            }
            const char* str = reinterpret_cast<const char*>(CDCDesc::DESC_STRING[index]);
            char_count = std::strlen(str);
            const size_t max_count = sizeof(desc_str) / sizeof(desc_str[0]) - 1;
            if (char_count > max_count)
            {
                char_count = max_count;
            }
            for (size_t i = 0; i < char_count; i++)
            {
                desc_str[1 + i] = str[i];
            }
            break;
        }
    }

    desc_str[0] = static_cast<uint16_t>((TUSB_DESC_STRING << 8) | (2 * char_count + 2));
    return desc_str;
}

const uint8_t* CaptureDevice::get_descriptor_device_cb()
{
    return reinterpret_cast<const uint8_t*>(&CDCDesc::DESC_DEVICE);
}

const uint8_t* CaptureDevice::get_hid_descriptor_report_cb(uint8_t itf)
{
    return nullptr;
}

const uint8_t* CaptureDevice::get_descriptor_configuration_cb(uint8_t index)
{
    return CDCDesc::DESC_CONFIG;
}

const uint8_t* CaptureDevice::get_descriptor_device_qualifier_cb()
{
    return nullptr;
}
