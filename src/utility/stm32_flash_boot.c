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

#ifdef ARDUINO_ARCH_STM32

#include "stm32_flash_boot.h"
#if defined(STM32F0xx)
#include <stm32f0xx_hal_flash_ex.h> // FLASH_PAGE_SIZE is in hal.h
#elif defined(STM32F1xx)
#include <stm32f1xx_hal_flash_ex.h> // FLASH_PAGE_SIZE is in hal.h
#elif defined(STM32F2xx)
#include <stm32f2xx.h>
#elif defined(STM32F3xx)
#include <stm32f3xx_hal_flash_ex.h> // FLASH_PAGE_SIZE is in hal.h
#elif defined(STM32F4xx)
#include "stm32f4xx.h"
#elif defined(STM32F7xx)
#include <stm32f7xx.h>
#define SMALL_SECTOR_SIZE 0x8000 // sectors 0 to 3
#define LARGE_SECTOR_SIZE 0x40000 // from sector 5
#endif

#if defined(STM32F2xx) || defined(STM32F4xx)
#define SMALL_SECTOR_SIZE 0x4000 // sectors 0 to 3
#define LARGE_SECTOR_SIZE 0x20000 // from sector 5
#endif

/**
 * function for bootload flash to flash
 * this function runs exclusively in RAM.
 * note: interrupts must be disabled.
 * note: parameters must be multiple of FLASH_PAGE_SIZE
 * note: for models with sectors, flash_offs must be start address of a sector
 */
void copy_flash_pages(uint32_t flash_offs, const uint8_t *data, uint32_t count, uint8_t reset) {

  uint32_t page_address = flash_offs;
  while (FLASH->SR & FLASH_SR_BSY);

#ifdef FLASH_CR_PSIZE
  CLEAR_BIT(FLASH->CR, FLASH_CR_PSIZE);
  FLASH->CR |= 0x00000200U; // FLASH_PSIZE_WORD;
  FLASH->CR |= FLASH_CR_PG;
#endif
#ifdef FLASH_PAGE_SIZE
  SET_BIT(FLASH->CR, FLASH_CR_PER);
  for (uint16_t i = 0; i < count / FLASH_PAGE_SIZE; i++) {
    WRITE_REG(FLASH->AR, page_address);
    SET_BIT(FLASH->CR, FLASH_CR_STRT);
    page_address += FLASH_PAGE_SIZE;
    while (FLASH->SR & FLASH_SR_BSY);
  }
  CLEAR_BIT(FLASH->CR, FLASH_CR_PER);
#else
  uint8_t startSector = (flash_offs == FLASH_BASE) ? 0 : 1; // 1 if bootloader is in sector 0
  uint8_t endSector = 1 + startSector + ((count - 1) / SMALL_SECTOR_SIZE); // size of the first 4 sectors is 0x4000 kB
  if (endSector > 4) { // if more than 4 sectors are needed (including bootloader)
    if (endSector < 8) { // size of sector 4 is (4 * SMALL_SECTOR_SIZE)
      endSector = 5;
    } else { // let's divide with the count of large sectors and add the first 5 sectors as one large sector
      endSector = 5 + (((count - 1) + (startSector * SMALL_SECTOR_SIZE)) / LARGE_SECTOR_SIZE);
    }
  }
  for (uint8_t sector = startSector; sector < endSector; sector++) {
    CLEAR_BIT(FLASH->CR, FLASH_CR_SNB);
    FLASH->CR |= FLASH_CR_SER | (sector << FLASH_CR_SNB_Pos);
    FLASH->CR |= FLASH_CR_STRT;
    while (FLASH->SR & FLASH_SR_BSY);
  }
  CLEAR_BIT(FLASH->CR, (FLASH_CR_SER | FLASH_CR_SNB));
#endif

  page_address = flash_offs;
  uint16_t* ptr = (uint16_t*) data;
  while (FLASH->SR & FLASH_SR_BSY);
#ifdef FLASH_CR_PSIZE
  CLEAR_BIT(FLASH->CR, FLASH_CR_PSIZE);
  FLASH->CR |= 0x00000100U; // FLASH_PSIZE_HALF_WORD
  FLASH->CR |= FLASH_CR_PG;
#else
  SET_BIT(FLASH->CR, FLASH_CR_PG);
#endif
  while (count) {
    *(volatile uint16_t*)page_address = *ptr;
    page_address += 2;
    count -= 2;
    ptr++;
    while (FLASH->SR & FLASH_SR_BSY);
  }
  CLEAR_BIT(FLASH->CR, FLASH_CR_PG);

  if (reset) {
    NVIC_SystemReset();
  }
}

#endif
