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

#if defined(ARDUINO_ARCH_RP2040)

#include <Arduino.h>

#include "InternalStorageRP2.h"

#include <hardware/flash.h>
#include "utility/rp2_flash_boot.h"

InternalStorageRP2Class::InternalStorageRP2Class() {
  maxSketchSize = MAX_FLASH / 2;
  maxSketchSize = (maxSketchSize / FLASH_SECTOR_SIZE) * FLASH_SECTOR_SIZE; // align to sector
  sectorAlignedLength = 0;
  pageBuffer = nullptr;
  pageBufferIndex = 0;
  flashWriteIndex = maxSketchSize;
}

int InternalStorageRP2Class::open(int length) {

  if (length > maxSketchSize)
    return 0;

  sectorAlignedLength = ((length / FLASH_SECTOR_SIZE) + 1) * FLASH_SECTOR_SIZE; // align to sector up

  noInterrupts();
  rp2040.idleOtherCore();
  flash_range_erase(maxSketchSize, sectorAlignedLength);
  rp2040.resumeOtherCore();
  interrupts();

  pageBufferIndex = 0;
  flashWriteIndex = maxSketchSize;

  return 1;
}

size_t InternalStorageRP2Class::write(uint8_t b) {

  if (pageBuffer == nullptr) {
    pageBuffer = new uint8_t[PAGE_SIZE];
    if (pageBuffer == nullptr)
      return 0;
  }

  pageBuffer[pageBufferIndex++] = b;

  if (pageBufferIndex == PAGE_SIZE) {
    noInterrupts();
    rp2040.idleOtherCore();
    flash_range_program(flashWriteIndex, pageBuffer, PAGE_SIZE);
    rp2040.resumeOtherCore();
    interrupts();
    pageBufferIndex = 0;
    flashWriteIndex += PAGE_SIZE;
  }

  return 1;
}

void InternalStorageRP2Class::close() {
  if (pageBufferIndex > 0) {
    memset(pageBuffer + pageBufferIndex, PAGE_SIZE - pageBufferIndex, 0xFF);
    noInterrupts();
    rp2040.idleOtherCore();
    flash_range_program(flashWriteIndex, pageBuffer, PAGE_SIZE);
    rp2040.resumeOtherCore();
    interrupts();
    pageBufferIndex = 0;
  }
}

void InternalStorageRP2Class::apply() {
  noInterrupts();
  rp2040.idleOtherCore();
  copy_flash_pages(SKETCH_START_ADDRESS, (uint8_t*) maxSketchSize + XIP_BASE, sectorAlignedLength, true);
}

InternalStorageRP2Class InternalStorage;

#endif
