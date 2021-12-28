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

#include <Arduino.h>

#include "InternalStorageSTM32.h"
#include "utility/stm32_flash_boot.h"

InternalStorageSTM32Class::InternalStorageSTM32Class() {
  maxSketchSize = (MAX_FLASH - SKETCH_START_ADDRESS) / 2;
  maxSketchSize = (maxSketchSize / PAGE_SIZE) * PAGE_SIZE; // align to sector
  storageStartAddress = FLASH_BASE + SKETCH_START_ADDRESS + maxSketchSize;
  pageAlignedLength = 0;
  writeIndex = 0;
  flashWriteAddress = storageStartAddress;
}

int InternalStorageSTM32Class::open(int length) {

  if (length > maxSketchSize)
    return 0;

  pageAlignedLength = ((length / PAGE_SIZE) + 1) * PAGE_SIZE; // align to sector up
  flashWriteAddress = storageStartAddress;
  writeIndex = 0;

  FLASH_EraseInitTypeDef EraseInitStruct;
  EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
  EraseInitStruct.Banks = FLASH_BANK_1;
  EraseInitStruct.PageAddress = flashWriteAddress;
  EraseInitStruct.NbPages = pageAlignedLength / PAGE_SIZE;

  uint32_t pageError = 0;
  return (HAL_FLASH_Unlock() == HAL_OK //
    && HAL_FLASHEx_Erase(&EraseInitStruct, &pageError) == HAL_OK);
}

size_t InternalStorageSTM32Class::write(uint8_t b) {

  addressData.u8[writeIndex] = b;
  writeIndex++;

  if (writeIndex == 8) {
    writeIndex = 0;

    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, flashWriteAddress, addressData.u64) != HAL_OK)
      return 0;
    flashWriteAddress += 8;
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

InternalStorageSTM32Class InternalStorage;

#endif
