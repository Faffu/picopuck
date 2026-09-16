#include "class/cdc/cdc_device.h"
#include "bsp/board_api.h"
#include "USBDevice/DeviceDriver/DeviceDriver.h"

uint16_t* DeviceDriver::get_string_descriptor(const char* value, uint8_t index)
{
    static uint16_t string_desc_buffer[32];
    size_t char_count;

    if ( index == 0 )
    {
        //Language id, stored as two little endian bytes.
        string_desc_buffer[1] = static_cast<uint16_t>(static_cast<uint8_t>(value[0]) | (static_cast<uint8_t>(value[1]) << 8));
        string_desc_buffer[0] = static_cast<uint16_t>((0x03 << 8) | 4);
        return string_desc_buffer;
    }
    else 
    {
        char_count = strlen(value);
        if (char_count > 31)
        {
            char_count = 31;
        }
    }
    for (uint8_t i = 0; i < char_count; i++)
    {
        string_desc_buffer[i + 1] = value[i];
    }

    string_desc_buffer[0] = static_cast<uint16_t>((0x03 << 8) | (2 * static_cast<uint8_t>(char_count) + 2));
    return string_desc_buffer;
}