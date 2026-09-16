#ifndef _NVS_TOOL_H_
#define _NVS_TOOL_H_

#include <cstdint>
#include <string>
#include <array>
#include <cstring>
#include <hardware/flash.h>
#include <pico/flash.h>
#include <pico/multicore.h>
#include <pico/mutex.h>

/* Define NVS_SECTORS (number of sectors to allocate to storage) either here or with CMake */

class NVSTool
{
public:
    static constexpr size_t   KEY_LEN_MAX = 16; //Including null terminator
    static constexpr size_t   VALUE_LEN_MAX = FLASH_PAGE_SIZE - KEY_LEN_MAX;
    static constexpr uint32_t MAX_ENTRIES = ((NVS_SECTORS * FLASH_SECTOR_SIZE) / FLASH_PAGE_SIZE) - 1;

    static NVSTool& get_instance()
    {
        static NVSTool instance;
        return instance;
    }

    bool write(const std::string& key, const void* value, size_t len)
    {
        if (!valid_args(key, len))
        {
            return false;
        }

        mutex_enter_blocking(&nvs_mutex_);

        for (uint32_t i = 1; i < MAX_ENTRIES; ++i) 
        {
            Entry* read_entry = get_entry(i);

            if (!is_valid_entry(read_entry) || std::strcmp(read_entry->key, key.c_str()) == 0)
            {
                update_entry(i, key, value, len);

                mutex_exit(&nvs_mutex_);
                return true;
            }
        }

        mutex_exit(&nvs_mutex_);
        return false; // No space for new entry
    }

    bool read(const std::string& key, void* value, size_t len)
    {
        if (!valid_args(key, len))
        {
            return false;
        }

        mutex_enter_blocking(&nvs_mutex_);

        for (uint32_t i = 1; i < MAX_ENTRIES; ++i) 
        {
            Entry* read_entry = get_entry(i);

            if (std::strcmp(read_entry->key, key.c_str()) == 0)
            {
                std::memcpy(value, read_entry->value, len);

                mutex_exit(&nvs_mutex_);
                return true;
            }
            else if (!is_valid_entry(read_entry))
            {
                // Key not found
                break;
            }
        }

        mutex_exit(&nvs_mutex_);
        return false;
    }

    void erase_all()
    {
        mutex_enter_blocking(&nvs_mutex_);
        run_flash_op(erase_all_op, nullptr);
        mutex_exit(&nvs_mutex_);
    }

private:
    NVSTool()
    {
        mutex_init(&nvs_mutex_);

        Entry* initial_entry = get_entry(0);

        if (std::strcmp(initial_entry->key, INVALID_KEY) != 0) 
        {
            erase_all();
        }
    }

    //Flash can't be read while it's being written, and the other core runs
    //from flash too. Once that core can be paused (multicore lockout victim)
    //pause it for the write; before it runs, write directly.
    static void run_flash_op(void (*op)(void*), void* arg)
    {
        if (multicore_lockout_victim_is_initialized(get_core_num() ^ 1) &&
            flash_safe_execute(op, arg, UINT32_MAX) == PICO_OK)
        {
            return;
        }
        op(arg);
    }

    static void erase_all_op(void*)
    {
        for (uint32_t i = 0; i < NVS_SECTORS; ++i) 
        {
            flash_range_erase(NVS_START_OFFSET + i * FLASH_SECTOR_SIZE, FLASH_SECTOR_SIZE);
        }

        Entry entry;

        for (uint32_t i = 0; i < MAX_ENTRIES + 1; ++i) 
        {
            flash_range_program(NVS_START_OFFSET + i * sizeof(Entry), 
                                reinterpret_cast<const uint8_t*>(&entry), 
                                sizeof(Entry));
        }
    }

    struct SectorWrite
    {
        uint32_t sector_offset;
        const uint8_t* data;
    };

    static void write_sector_op(void* arg)
    {
        const SectorWrite* write = static_cast<const SectorWrite*>(arg);
        flash_range_erase(write->sector_offset, FLASH_SECTOR_SIZE);
        flash_range_program(write->sector_offset, write->data, FLASH_SECTOR_SIZE);
    }

    ~NVSTool() = default;
    NVSTool(const NVSTool&) = delete;
    NVSTool& operator=(const NVSTool&) = delete;

    struct Entry 
    {
        char key[KEY_LEN_MAX];
        uint8_t value[VALUE_LEN_MAX];

        Entry()
        {
            std::fill(std::begin(key), std::end(key), '\0');
            std::strncpy(key, INVALID_KEY, sizeof(key) - 1);
            std::fill(std::begin(value), std::end(value), 0xFF);
        }
    };
    static_assert(sizeof(Entry) == FLASH_PAGE_SIZE, "NVSTool::Entry size mismatch");

    static constexpr const char INVALID_KEY[KEY_LEN_MAX] = "INVALID";
    static constexpr uint32_t NVS_START_OFFSET = PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE * NVS_SECTORS;

    mutex_t nvs_mutex_;

    inline Entry* get_entry(const uint32_t index) 
    {
        return reinterpret_cast<Entry*>(XIP_BASE + NVS_START_OFFSET + index * sizeof(Entry));
    }

    inline bool is_valid_entry(const Entry* entry) 
    {
        return std::strcmp(entry->key, INVALID_KEY) != 0;
    }

    inline bool valid_args(const std::string& key, size_t len)
    {
        return (key.size() < KEY_LEN_MAX - 1 && len <= sizeof(Entry::value) && std::strcmp(key.c_str(), INVALID_KEY) != 0);
    }

    void update_entry(uint32_t index, const std::string& key, const void* buffer, size_t len)
    {
        uint32_t entry_offset = index * sizeof(Entry);
        uint32_t sector_offset = ((NVS_START_OFFSET + entry_offset) / FLASH_SECTOR_SIZE) * FLASH_SECTOR_SIZE;

        std::array<uint8_t, FLASH_SECTOR_SIZE> sector_buffer;
        std::memcpy(sector_buffer.data(), reinterpret_cast<const uint8_t*>(XIP_BASE + sector_offset), FLASH_SECTOR_SIZE);

        Entry* entry_to_write = reinterpret_cast<Entry*>(sector_buffer.data() + entry_offset);

        *entry_to_write = Entry();
        std::strncpy(entry_to_write->key, key.c_str(), key.size());
        entry_to_write->key[key.size()] = '\0';
        std::memcpy(entry_to_write->value, buffer, len);

        SectorWrite write{ sector_offset, sector_buffer.data() };
        run_flash_op(write_sector_op, &write);
    }

}; // class NVSTool

#endif // _NVS_TOOL_H_