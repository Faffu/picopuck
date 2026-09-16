#ifndef _REPORT_CAPTURE_H_
#define _REPORT_CAPTURE_H_

#include <atomic>
#include <cstddef>
#include <cstdint>

/*  Holds raw Bluetooth reports, and Bluetooth stack log text, between the
    radio and the USB side.

    The Bluetooth stack runs on core1 and the USB device on core0, so this is a
    single producer, single consumer ring. A full ring drops the newest report
    and counts it, which keeps the radio side free of any wait.  */

namespace ReportCapture {

static constexpr size_t MAX_REPORT_LEN = 64;
static constexpr size_t CAPACITY = 256;

enum Kind : uint8_t { KIND_REPORT = 0, KIND_TEXT = 1 };

struct Entry
{
    uint32_t timestamp_us;
    uint8_t kind;
    uint8_t len;
    uint8_t data[MAX_REPORT_LEN];
};

//Consumer side. Reports are only stored while enabled, so an unread ring
//doesn't fill up with reports and crowd out log text. Starts disabled.
void set_reports_enabled(bool enabled);

//Producer side. Reports longer than MAX_REPORT_LEN are stored truncated.
void push(const uint8_t* data, size_t len, uint32_t timestamp_us);

//Producer side. Text is split into MAX_REPORT_LEN chunks, printed verbatim.
void push_text(const char* text, size_t len);

//Consumer side. False when there is nothing to read.
bool pop(Entry& entry);

//Reports dropped because the ring was full, since boot.
uint32_t dropped();

//Reports truncated because they were longer than MAX_REPORT_LEN, since boot.
uint32_t truncated();

void reset();

} // namespace ReportCapture

#endif // _REPORT_CAPTURE_H_
