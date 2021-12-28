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

#if defined(STM32F1xx)

#include "stm32_flash_boot.h"
#include <stm32f1xx_hal_flash_ex.h> // FLASH_PAGE_SIZE is in hal.h

/**
 * function for bootload flash to flash
 * this function runs exclusively in RAM.
 * note: interrupts must be disabled.
 * note: parameters must be multiple of FLASH_PAGE_SIZE
 */
void copy_flash_pages(uint32_t flash_offs, const uint8_t *data, uint32_t count, uint8_t reset) {

  uint32_t page_address = flash_offs;
  while (FLASH->SR & FLASH_SR_BSY);
  SET_BIT(FLASH->CR, FLASH_CR_PER);
  for (uint16_t i = 0; i < count / FLASH_PAGE_SIZE; i++) {
    WRITE_REG(FLASH->AR, page_address);
    SET_BIT(FLASH->CR, FLASH_CR_STRT);
    page_address += FLASH_PAGE_SIZE;
    while (FLASH->SR & FLASH_SR_BSY);
  }
  CLEAR_BIT(FLASH->CR, FLASH_CR_PER);

  page_address = flash_offs;
  uint16_t* ptr = (uint16_t*) data;
  while (FLASH->SR & FLASH_SR_BSY);
  SET_BIT(FLASH->CR, FLASH_CR_PG);
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
