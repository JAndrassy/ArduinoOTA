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

#ifndef OTA_STORAGE_STM32_SECTOR
#define OTA_STORAGE_STM32_SECTOR 5
#endif

#include <Arduino.h>

#include "InternalStorageSTM32.h"
#include "utility/stm32_flash_boot.h"

#if defined(STM32F7xx)
const uint32_t SECTOR_SIZE = 0x40000; // from sector 5
#elif defined(STM32F2xx) || defined(STM32F4xx)
const uint32_t SECTOR_SIZE = 0x20000; // from sector 5 (for F2, F4)
#endif

InternalStorageSTM32Class::InternalStorageSTM32Class(uint8_t _sector) {
  sector = _sector < 5 ? 5 : _sector;
#ifdef FLASH_TYPEERASE_SECTORS
  maxSketchSize = SECTOR_SIZE * (sector - 4); // sum of sectors 0 to 4 is 128kB, starting sector 5 the sector size is 128kB
  storageStartAddress = FLASH_BASE + maxSketchSize;
  maxSketchSize -= SKETCH_START_ADDRESS;
  if (MAX_FLASH - maxSketchSize < maxSketchSize) {
    maxSketchSize = MAX_FLASH - maxSketchSize;
  }
#else
  maxSketchSize = (MAX_FLASH - SKETCH_START_ADDRESS) / 2;
  maxSketchSize = (maxSketchSize / PAGE_SIZE) * PAGE_SIZE; // align to page
  storageStartAddress = FLASH_BASE + SKETCH_START_ADDRESS + maxSketchSize;
#endif
  pageAlignedLength = 0;
  writeIndex = 0;
  flashWriteAddress = storageStartAddress;
}

int InternalStorageSTM32Class::open(int length) {

  if (length > maxSketchSize)
    return 0;

  pageAlignedLength = ((length / PAGE_SIZE) + 1) * PAGE_SIZE; // align to page up
  flashWriteAddress = storageStartAddress;
  writeIndex = 0;

  FLASH_EraseInitTypeDef EraseInitStruct;
#ifdef FLASH_TYPEERASE_SECTORS
  EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
  EraseInitStruct.Sector = sector;
  EraseInitStruct.NbSectors = 1 + (length / SECTOR_SIZE);
  EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
#else
  EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
  EraseInitStruct.PageAddress = flashWriteAddress;
  EraseInitStruct.NbPages = pageAlignedLength / PAGE_SIZE;
#endif
#ifdef FLASH_BANK_1
  EraseInitStruct.Banks = FLASH_BANK_1;
#endif

  uint32_t pageError = 0;
  return (HAL_FLASH_Unlock() == HAL_OK //
    && HAL_FLASHEx_Erase(&EraseInitStruct, &pageError) == HAL_OK);
}

size_t InternalStorageSTM32Class::write(uint8_t b) {

  addressData.u8[writeIndex] = b;
  writeIndex++;

  if (writeIndex == 4) {
    writeIndex = 0;

    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, flashWriteAddress, addressData.u32) != HAL_OK)
      return 0;
    flashWriteAddress += 4;
  }
  return 1;
}

void InternalStorageSTM32Class::close() {
  while (writeIndex) {
    write(0xff);
  }
  HAL_FLASH_Lock();
}

void InternalStorageSTM32Class::apply() {
  if (HAL_FLASH_Unlock() != HAL_OK)
    return;
  noInterrupts();
  copy_flash_pages(FLASH_BASE + SKETCH_START_ADDRESS, (uint8_t*) storageStartAddress, pageAlignedLength, true);
}

InternalStorageSTM32Class InternalStorage(OTA_STORAGE_STM32_SECTOR);

#endif
