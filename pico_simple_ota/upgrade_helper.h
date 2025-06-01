
/* MIT License

Copyright (c) 2024 Adrian Cruceru - https://github.com/AdrianCX/pico_https

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#pragma once

#include "general_config.h"

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "http_session.h"
#include "hardware/flash.h"

// Should be defined in general config
#ifndef OTA_FLASH_BUFFER_OFFSET
#define OTA_FLASH_BUFFER_OFFSET 0x100000
#endif

#ifndef MAX_BINARY_SIZE
#define MAX_BINARY_SIZE (256 * 4096)
#endif

class UpgradeHelper
{
public:
    bool clear();
    void upgrade();

    bool storeData(u8_t *data, size_t len);
    static void writeToFlash(void *self);

    inline void setRegionErased(uint32_t region) { m_regionErased[region/8] |= (1 << (region % 8)); }
    inline bool getRegionErased(uint32_t region) { return (m_regionErased[region/8] & (1 << (region % 8))) != 0; }

    static const int NUM_REGIONS = MAX_BINARY_SIZE / FLASH_SECTOR_SIZE;

    uint8_t *getRegions() { return &m_regionErased[0]; }
    
private:
    uint16_t m_index = 0;

    static const int BLOCK_SIZE = 512;
    uint8_t m_buffer[BLOCK_SIZE] = {};

    uint8_t m_regionErased[NUM_REGIONS/8] = {};
};
