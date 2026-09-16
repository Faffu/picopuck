#include <cstring>

#include "SteamController2/ReportCapture.h"

namespace ReportCapture {

namespace {

Entry ring_[CAPACITY];
std::atomic<uint32_t> write_index_{0};
std::atomic<uint32_t> read_index_{0};
std::atomic<uint32_t> dropped_{0};
std::atomic<uint32_t> truncated_{0};
std::atomic<bool> reports_enabled_{false};

bool store(Kind kind, const uint8_t* data, size_t len, uint32_t timestamp_us)
{

    const uint32_t write = write_index_.load(std::memory_order_relaxed);
    const uint32_t read = read_index_.load(std::memory_order_acquire);

    if (write - read >= CAPACITY)
    {
        dropped_.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    Entry& entry = ring_[write % CAPACITY];
    entry.timestamp_us = timestamp_us;
    entry.kind = kind;
    entry.len = static_cast<uint8_t>(len);
    std::memcpy(entry.data, data, len);

    write_index_.store(write + 1, std::memory_order_release);
    return true;
}

} // namespace

void set_reports_enabled(bool enabled)
{
    reports_enabled_.store(enabled, std::memory_order_relaxed);
}

void push(const uint8_t* data, size_t len, uint32_t timestamp_us)
{
    if (!data || len == 0 || !reports_enabled_.load(std::memory_order_relaxed))
    {
        return;
    }

    if (len > MAX_REPORT_LEN)
    {
        len = MAX_REPORT_LEN;
        truncated_.fetch_add(1, std::memory_order_relaxed);
    }
    store(KIND_REPORT, data, len, timestamp_us);
}

void push_text(const char* text, size_t len)
{
    for (size_t off = 0; text && off < len; off += MAX_REPORT_LEN)
    {
        const size_t chunk = (len - off < MAX_REPORT_LEN) ? len - off : MAX_REPORT_LEN;
        if (!store(KIND_TEXT, reinterpret_cast<const uint8_t*>(&text[off]), chunk, 0))
        {
            return; //rest of this text would be dropped too
        }
    }
}

bool pop(Entry& entry)
{
    const uint32_t read = read_index_.load(std::memory_order_relaxed);
    if (read == write_index_.load(std::memory_order_acquire))
    {
        return false;
    }

    entry = ring_[read % CAPACITY];
    read_index_.store(read + 1, std::memory_order_release);
    return true;
}

uint32_t dropped()
{
    return dropped_.load(std::memory_order_relaxed);
}

uint32_t truncated()
{
    return truncated_.load(std::memory_order_relaxed);
}

void reset()
{
    read_index_.store(0, std::memory_order_relaxed);
    write_index_.store(0, std::memory_order_relaxed);
    dropped_.store(0, std::memory_order_relaxed);
    truncated_.store(0, std::memory_order_relaxed);
    reports_enabled_.store(false, std::memory_order_relaxed);
}

} // namespace ReportCapture
