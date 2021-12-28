/*
  Copyright (c) 2021 Juraj Andrassy

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifdef ARDUINO_ARCH_RP2040

#include "rp2_flash_boot.h"

#include <hardware/flash.h>
#include <pico/bootrom.h>
#include <RP2040.h> // CMSIS
#include <hardware/structs/ssi.h> // XIP_BASE defintion

// boot2 functions copied from hardware/flash.c

#define BOOT2_SIZE_WORDS 64

static uint32_t boot2_copyout[BOOT2_SIZE_WORDS];
static bool boot2_copyout_valid = false;

static void __no_inline_not_in_flash_func(flash_init_boot2_copyout)(void) {
    if (boot2_copyout_valid)
        return;
    for (int i = 0; i < BOOT2_SIZE_WORDS; ++i)
        boot2_copyout[i] = ((uint32_t *)XIP_BASE)[i];
    __compiler_memory_barrier();
    boot2_copyout_valid = true;
}

static void __no_inline_not_in_flash_func(flash_enable_xip_via_boot2)(void) {
    ((void (*)(void))boot2_copyout+1)();
}

#define FLASH_BLOCK_ERASE_CMD 0xd8

// end of copied from flash.c

/**
 * function for bootload flash to flash
 * this function runs exclusively in RAM and after erasing the requested sectors
 * only calls functions from ROM. (rom_func_lookup() is in flash!)
 * note: interrupts and second core must be disabled.
 * note: parameters must be multiple of FLASH_SECTOR_SIZE
 * the function is based on functions in flash.c
 */
void __no_inline_not_in_flash_func(copy_flash_pages)(uint32_t flash_offs, const uint8_t *data, size_t count, bool reset) {
    void (*connect_internal_flash)(void) = (void(*)(void))rom_func_lookup(rom_table_code('I', 'F'));
    void (*flash_exit_xip)(void) = (void(*)(void))rom_func_lookup(rom_table_code('E', 'X'));
    void (*flash_range_erase)(uint32_t, size_t, uint32_t, uint8_t) =
        (void(*)(uint32_t, size_t, uint32_t, uint8_t))rom_func_lookup(rom_table_code('R', 'E'));
    void (*flash_range_program)(uint32_t, const uint8_t*, size_t) =
        (void(*)(uint32_t, const uint8_t*, size_t))rom_func_lookup(rom_table_code('R', 'P'));
    void (*flash_flush_cache)(void) = (void(*)(void))rom_func_lookup(rom_table_code('F', 'C'));
    void (*memcpy_fast)(uint8_t*, uint8_t*, uint32_t) =
        (void(*)(uint8_t*, uint8_t*, uint32_t))rom_func_lookup(rom_table_code('M','C'));
    flash_init_boot2_copyout();

    __compiler_memory_barrier();

    connect_internal_flash();
    flash_exit_xip();
    flash_range_erase(flash_offs, count, FLASH_BLOCK_SIZE, FLASH_BLOCK_ERASE_CMD);
    flash_flush_cache();
    flash_enable_xip_via_boot2();

    uint8_t buff[FLASH_PAGE_SIZE];
    while (count) {
      memcpy_fast(buff, (char*) data, FLASH_PAGE_SIZE);
      connect_internal_flash();
      flash_exit_xip();
      flash_range_program(flash_offs, buff, FLASH_PAGE_SIZE);
      flash_flush_cache();
      flash_enable_xip_via_boot2();
      flash_offs += FLASH_PAGE_SIZE;
      data += FLASH_PAGE_SIZE;
      count -= FLASH_PAGE_SIZE;
    }
    if (reset) {
      NVIC_SystemReset();
    }
}

#endif
