/*
 Copyright (c) 2018 Juraj Andrassy

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

#include <Arduino.h>

#if defined(__AVR__) && FLASHEND >= 0xFFFF

#include "InternalStorageAVR.h"
#include "utility/optiboot.h"

InternalStorageAVRClass::InternalStorageAVRClass() {
  maxSketchSize = (MAX_FLASH - bootloaderSize) / 2;
  maxSketchSize = (maxSketchSize / SPM_PAGESIZE) * SPM_PAGESIZE; // align to page
  pageAddress = maxSketchSize;
  pageIndex = 0;
}

int InternalStorageAVRClass::open(int length) {
  (void)length;
  pageAddress = maxSketchSize;
  pageIndex = 0;
  return 1;
}

size_t InternalStorageAVRClass::write(uint8_t b) {
  if (pageIndex == 0) {
    optiboot_page_erase(pageAddress);
  }
  dataWord.u8[pageIndex % 2] = b;
  if (pageIndex % 2) {
    optiboot_page_fill(pageAddress + pageIndex, dataWord.u16);
  }
  pageIndex++;
  if (pageIndex == SPM_PAGESIZE) {
    optiboot_page_write(pageAddress);
    pageIndex = 0;
    pageAddress += SPM_PAGESIZE;
  }
  return 1;
}

void InternalStorageAVRClass::close() {
  if (pageIndex) {
    optiboot_page_write(pageAddress);
  }
  pageIndex = 0;
}

void InternalStorageAVRClass::clear() {
}

void InternalStorageAVRClass::apply() {
  copy_flash_pages_cli(SKETCH_START_ADDRESS, maxSketchSize, (pageAddress - maxSketchSize) / SPM_PAGESIZE + 1, true);
}

long InternalStorageAVRClass::maxSize() {
  return maxSketchSize;
}

InternalStorageAVRClass InternalStorage;

#endif
