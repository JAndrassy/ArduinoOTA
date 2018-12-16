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

 WiFi101OTA version Feb 2017
 by Sandeep Mistry (Arduino)
 modified for ArduinoOTA Dec 2018
 by Juraj Andrassy
*/

#include <Arduino.h>

#include "InternalStorage.h"

InternalStorageClass::InternalStorageClass() :
  MAX_PARTIONED_SKETCH_SIZE((MAX_FLASH - SKETCH_START_ADDRESS) / 2),
  STORAGE_START_ADDRESS(SKETCH_START_ADDRESS + MAX_PARTIONED_SKETCH_SIZE)
{
  _writeIndex = 0;
  _writeAddress = nullptr;
}

extern "C" {
  // these functions must be in RAM (.data) and NOT inlined
  // as they erase and copy the sketch data in flash

  __attribute__ ((long_call, noinline, section (".data#"))) //
  static void waitForReady() {
    while (!NVMCTRL->INTFLAG.bit.READY);
  }

  __attribute__ ((long_call, noinline, section (".data#")))
  static void eraseFlash(int address, int length, int pageSize)
  {
    int rowSize = pageSize * 4;
    for (int i = 0; i < length; i += rowSize) {
      NVMCTRL->ADDR.reg = ((uint32_t)(address + i)) / 2;
      NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMDEX_KEY | NVMCTRL_CTRLA_CMD_ER;
      
      waitForReady();
    }
  }

  __attribute__ ((long_call, noinline, section (".data#")))
  static void copyFlashAndReset(int dest, int src, int length, int pageSize)
  {
    uint32_t* d = (uint32_t*)dest;
    uint32_t* s = (uint32_t*)src;

    eraseFlash(dest, length, pageSize);

    for (int i = 0; i < length; i += 4) {
      *d++ = *s++;

      waitForReady();
    }

    NVIC_SystemReset();
  }
}

int InternalStorageClass::open(int length)
{
  (void)length;
  _writeIndex = 0;
  _writeAddress = (uint32_t*)STORAGE_START_ADDRESS;

  // enable auto page writes
  NVMCTRL->CTRLB.bit.MANW = 0;

  eraseFlash(STORAGE_START_ADDRESS, MAX_PARTIONED_SKETCH_SIZE, PAGE_SIZE);

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

    waitForReady();
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

  copyFlashAndReset(SKETCH_START_ADDRESS, STORAGE_START_ADDRESS, MAX_PARTIONED_SKETCH_SIZE, PAGE_SIZE);
}

long InternalStorageClass::maxSize()
{
  return MAX_PARTIONED_SKETCH_SIZE;
}

InternalStorageClass InternalStorage;
