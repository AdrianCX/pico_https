
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
#include <string.h>
#include <time.h>

#include "hardware/flash.h"
#include "hardware/xip_cache.h"
#include "pico/bootrom.h"
#include "pico/cyw43_arch.h"
#include "pico/flash.h"
#include "pico/stdlib.h"

#include "pico_logger.h"
#include "uf2.h"

#include "upgrade_helper.h"

#define FLASH_BLOCK_ERASE_CMD 0xd8

static void __no_inline_not_in_flash_func(upgrade_binary)(void *param)
{
  save_and_disable_interrupts();
  uint8_t buffer[FLASH_SECTOR_SIZE];

  UpgradeHelper *upgradeHelper = (UpgradeHelper *)param;
  uint8_t *regionErased = upgradeHelper->getRegions();

  // From this point on, only ROM functions are called, execution is from RAM.
  rom_connect_internal_flash_fn flash_connect_internal_func = (rom_connect_internal_flash_fn) rom_func_lookup(ROM_FUNC_CONNECT_INTERNAL_FLASH);
  rom_flash_exit_xip_fn flash_exit_xip_func = (rom_flash_exit_xip_fn) rom_func_lookup(ROM_FUNC_FLASH_EXIT_XIP);
  rom_flash_flush_cache_fn flash_flush_cache_func = (rom_flash_flush_cache_fn) rom_func_lookup(ROM_FUNC_FLASH_FLUSH_CACHE);
  rom_flash_range_program_fn flash_range_program_func = (rom_flash_range_program_fn)rom_func_lookup_inline(ROM_FUNC_FLASH_RANGE_PROGRAM);
  rom_flash_range_erase_fn flash_range_erase_func = (rom_flash_range_erase_fn)rom_func_lookup_inline(ROM_FUNC_FLASH_RANGE_ERASE);

  __compiler_memory_barrier();
  flash_connect_internal_func();
  flash_exit_xip_func();
  for (uint32_t i=0; i<UpgradeHelper::NUM_REGIONS;i++)
  {
      if ((regionErased[i/8] & (1 << (i % 8))) != 0)
      {
          uint32_t offset = i * FLASH_SECTOR_SIZE;
          flash_range_erase_func(offset, FLASH_SECTOR_SIZE, FLASH_BLOCK_SIZE, FLASH_BLOCK_ERASE_CMD);
      }
  }
  __compiler_memory_barrier();

  for (uint32_t i=0; i<UpgradeHelper::NUM_REGIONS;i++)
  {
      if ((regionErased[i/8] & (1 << (i % 8))) != 0)
      {
          uint32_t offset = i * FLASH_SECTOR_SIZE;
          const uint8_t *flash_target_contents = (const uint8_t *) (XIP_BASE + offset + OTA_FLASH_BUFFER_OFFSET);
          for (uint32_t j=0;j<FLASH_SECTOR_SIZE;++j)
          {
              buffer[j] = flash_target_contents[j];
          }
          
          __compiler_memory_barrier();
          flash_range_program_func(offset, &buffer[0], FLASH_SECTOR_SIZE);
      }
  }
  flash_flush_cache_func();
  __compiler_memory_barrier();
  
  while (true)
  {
      #define AIRCR_Register (*((volatile uint32_t*)(PPB_BASE + 0x0ED0C)))
      AIRCR_Register = 0x5FA0004;
  }
}

bool UpgradeHelper::clear()
{
    trace("UpgradeHelper::clear: this=%p", this);
    memset(&m_regionErased[0], 0, sizeof(m_regionErased));
    m_index = 0;
    return true;
}

bool UpgradeHelper::storeData(u8_t *data, size_t len)
{
    while (len > 0)
    {
        size_t available = (BLOCK_SIZE - m_index);
        size_t count = (len < available) ? len : available;
        
        memcpy(&m_buffer[m_index], data, count);
        m_index += count;
        
        len -= count;
        data += count;
        
        if (m_index == BLOCK_SIZE)
        {
            m_index = 0;
            
            UF2_Block *block = (UF2_Block *)&m_buffer[0];
            if (block->magicEnd != UF2_MAGIC_END)
            {
                trace("UpgradeHelper::storeData: this=%p targetAddr[0x%x] flags[0x%x] payloadSize[%d] blockNo[%d] numBlocks[%d] magicEnd[0x%x] bad magic.\n", this, block->targetAddr, block->flags, block->payloadSize, block->blockNo, block->numBlocks, block->magicEnd);
                return false;
            }

            if ((block->flags & UF2_FLAG_NOFLASH) != 0)
            {
                // ignore not-in-flash segment
                trace("UpgradeHelper::storeData: this=%p targetAddr[0x%x] flags[0x%x] payloadSize[%d] blockNo[%d] numBlocks[%d] magicEnd[0x%x] segment not in flash.\n", this, block->targetAddr, block->flags, block->payloadSize, block->blockNo, block->numBlocks, block->magicEnd);
                continue;
            }

            if ((block->flags & UF2_FLAG_EXTENSION_TAGS) != 0)
            {
                // Should add parsing for extension tags in the future and ignore if "UF2_EXTENSION_RP2_IGNORE_BLOCK" is set.
                trace("UpgradeHelper::storeData: this=%p targetAddr[0x%x] flags[0x%x] payloadSize[%d] blockNo[%d] numBlocks[%d] magicEnd[0x%x] segment with extension tags ignored for now.\n", this, block->targetAddr, block->flags, block->payloadSize, block->blockNo, block->numBlocks, block->magicEnd);
                continue;
            }

            block->targetAddr -= XIP_BASE;
            if (block->targetAddr >= MAX_BINARY_SIZE)
            {
                trace("UpgradeHelper::storeData: this=%p targetAddr[0x%x] flags[0x%x] payloadSize[%d] blockNo[%d] numBlocks[%d] magicEnd[0x%x] provided uf2 is too large.\n", this, block->targetAddr, block->flags, block->payloadSize, block->blockNo, block->numBlocks, block->magicEnd);
                return false;
            }
            
            block->targetAddr += OTA_FLASH_BUFFER_OFFSET;
            
            if ((block->targetAddr % 256) != 0)
            {
                trace("UpgradeHelper::storeData: this=%p targetAddr[0x%x] flags[0x%x] payloadSize[%d] blockNo[%d] numBlocks[%d] magicEnd[0x%x] targetAddress must be 256 byte aligned.\n", this, block->targetAddr, block->flags, block->payloadSize, block->blockNo, block->numBlocks, block->magicEnd);
                return false;
            }
            
            if ((block->payloadSize % 256) != 0)
            {
                trace("UpgradeHelper::storeData: this=%p targetAddr[0x%x] flags[0x%x] payloadSize[%d] blockNo[%d] numBlocks[%d] magicEnd[0x%x] payloadSize must be 256 byte aligned.\n", this, block->targetAddr, block->flags, block->payloadSize, block->blockNo, block->numBlocks, block->magicEnd);
                return false;
            }

            uint32_t region = (block->targetAddr - OTA_FLASH_BUFFER_OFFSET) / FLASH_SECTOR_SIZE;
            uint32_t offset = block->targetAddr - (block->targetAddr % FLASH_SECTOR_SIZE);
            if (region >= NUM_REGIONS)
            {
                trace("UpgradeHelper::storeData: this=%p targetAddr[0x%x] flags[0x%x] payloadSize[%d] blockNo[%d] numBlocks[%d] magicEnd[0x%x] region[%d] offset[0x%x] region outside NUM_REGIONS[%d].\n", this, block->targetAddr, block->flags, block->payloadSize, block->blockNo, block->numBlocks, block->magicEnd, region, offset, NUM_REGIONS);
                return false;
            }

            int rc = flash_safe_execute(UpgradeHelper::writeToFlash, (void*)this, UINT32_MAX);
            if (rc != PICO_OK)
            {
                trace("UpgradeHelper::storeData: this=%p targetAddr[0x%x] flags[0x%x] payloadSize[%d] blockNo[%d] numBlocks[%d] magicEnd[0x%x] failed write to flash.\n", this, block->targetAddr, block->flags, block->payloadSize, block->blockNo, block->numBlocks, block->magicEnd);
                return false;
            }
            
            const volatile uint8_t *flash_target_contents = (const uint8_t *) (XIP_BASE + block->targetAddr);
            for (uint32_t i=0;i<block->payloadSize;++i)
            {
                if (flash_target_contents[i] != block->data[i])
                {
                    trace("UpgradeHelper::storeData: this=%p targetAddr[0x%x] flags[0x%x] payloadSize[%d] blockNo[%d] numBlocks[%d] magicEnd[0x%x] written data does not match buffer, byte[0x%x] flash[0x%x] expected[0x%x].\n", this, block->targetAddr, block->flags, block->payloadSize, block->blockNo, block->numBlocks, block->magicEnd, i, flash_target_contents[i], block->data[i]);
                    return false;
                }
            }
        }
        else if (m_index > BLOCK_SIZE)
        {
            return false;
        }
    }

    return true;
}

void UpgradeHelper::writeToFlash(void *param)
{
    UpgradeHelper *self = (UpgradeHelper *)param;
    
    UF2_Block *block = (UF2_Block *)&self->m_buffer[0];

    uint32_t region = (block->targetAddr - OTA_FLASH_BUFFER_OFFSET) / FLASH_SECTOR_SIZE;
    uint32_t offset = block->targetAddr - (block->targetAddr % FLASH_SECTOR_SIZE);
    
    if (!self->getRegionErased(region))
    {
        self->setRegionErased(region);
        flash_range_erase(offset, FLASH_SECTOR_SIZE);
    }

    flash_range_program(block->targetAddr, &block->data[0], block->payloadSize);
}



void UpgradeHelper::upgrade()
{
    upgrade_binary((void *)this);
}

