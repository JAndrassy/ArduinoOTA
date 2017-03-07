/*
  Copyright (c) 2017 Arduino LLC.  All right reserved.

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

#include "InternalStorage.h"

#define PAGE_SIZE                   (64)
#define PAGES                       (4096)
#define MAX_FLASH                   (PAGE_SIZE * PAGES)
#define ROW_SIZE                    (PAGE_SIZE * 4)

#define SKETCH_START_ADDRESS        (0x2000)
#define MAX_PARTIONED_SKETCH_SIZE   ((MAX_FLASH - SKETCH_START_ADDRESS) / 2)
#define STORAGE_START_ADDRESS       (SKETCH_START_ADDRESS + MAX_PARTIONED_SKETCH_SIZE)

extern "C" {
  // these functions must be in RAM (.data) and NOT inlined
  // as they erase and copy the sketch data in flash

  __attribute__ ((long_call, noinline, section (".data#")))
  static void eraseFlash(int address, int length)
  {
    for (int i = 0; i < length; i += ROW_SIZE) {
      NVMCTRL->ADDR.reg = ((uint32_t)(address + i)) / 2;
      NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMDEX_KEY | NVMCTRL_CTRLA_CMD_ER;
      
      while (!NVMCTRL->INTFLAG.bit.READY);
    }
  }

  __attribute__ ((long_call, noinline, section (".data#")))
  static void copyFlashAndReset(int dest, int src, int length)
  {
    uint32_t* d = (uint32_t*)dest;
    uint32_t* s = (uint32_t*)src;

    eraseFlash(dest, length);

    for (int i = 0; i < length; i += 4) {
      *d++ = *s++;

      while (!NVMCTRL->INTFLAG.bit.READY);
    }

    NVIC_SystemReset();
  }
}

int InternalStorageClass::open()
{
  _writeIndex = 0;
  _writeAddress = (uint32_t*)STORAGE_START_ADDRESS;

  // enable auto page writes
  NVMCTRL->CTRLB.bit.MANW = 0;

  eraseFlash(STORAGE_START_ADDRESS, MAX_PARTIONED_SKETCH_SIZE);

  return 1;
}

size_t InternalStorageClass::write(uint8_t b)
{
  _addressData.u8[_writeIndex] = b;
  _writeIndex++;

  if (_writeIndex == 4) {
    _writeIndex = 0;

    *_writeAddress = _addressData.u32;

    _writeAddress++;

    while (!NVMCTRL->INTFLAG.bit.READY);
  }

  return 1;
}

void InternalStorageClass::close()
{
  while ((int)_writeAddress % PAGE_SIZE) {
    write(0xff);
  }
}

void InternalStorageClass::clear()
{
}

void InternalStorageClass::apply()
{
  // disable interrupts, as vector table will be erase during flash sequence
  noInterrupts();

  copyFlashAndReset(SKETCH_START_ADDRESS, STORAGE_START_ADDRESS, MAX_PARTIONED_SKETCH_SIZE);
}

long InternalStorageClass::maxSize()
{
  return MAX_PARTIONED_SKETCH_SIZE;
}

InternalStorageClass InternalStorage;
