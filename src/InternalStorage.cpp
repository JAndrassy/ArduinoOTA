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

#if defined(ARDUINO_ARCH_SAMD) || defined(ARDUINO_ARCH_NRF5)

#include <Arduino.h>

#include "InternalStorage.h"

InternalStorageClass::InternalStorageClass() :
  MAX_PARTIONED_SKETCH_SIZE((MAX_FLASH - SKETCH_START_ADDRESS) / 2),
  STORAGE_START_ADDRESS(SKETCH_START_ADDRESS + MAX_PARTIONED_SKETCH_SIZE)
{
  pageAlignedLength = 0;
  _writeIndex = 0;
  _writeAddress = nullptr;
}

void InternalStorageClass::debugPrint() {
  Serial.print("SKETCH_START_ADDRESS ");
  Serial.println(SKETCH_START_ADDRESS);
  Serial.print("PAGE_SIZE ");
  Serial.println(PAGE_SIZE);
  Serial.print("MAX_FLASH ");
  Serial.println(MAX_FLASH);
  Serial.print("MAX_PARTIONED_SKETCH_SIZE ");
  Serial.println(MAX_PARTIONED_SKETCH_SIZE);
  Serial.print("STORAGE_START_ADDRESS ");
  Serial.println(STORAGE_START_ADDRESS);
}

extern "C" {
  // these functions must be in RAM (.data) and NOT inlined
  // as they erase and copy the sketch data in flash

  __attribute__ ((long_call, noinline, section (".data#"))) //
  void waitForReady() {
#if defined(__SAMD51__)
    while (!NVMCTRL->STATUS.bit.READY);
#elif defined(ARDUINO_ARCH_SAMD)
    while (!NVMCTRL->INTFLAG.bit.READY);
#elif defined(ARDUINO_ARCH_NRF5)
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy);
#endif
  }
  
#if defined(__SAMD51__)
// Invalidate all CMCC cache entries if CMCC cache is enabled.
  __attribute__ ((long_call, noinline, section (".data#")))
  static void invalidate_CMCC_cache()
  {
    if (CMCC->SR.bit.CSTS) {
      CMCC->CTRL.bit.CEN = 0;
      while (CMCC->SR.bit.CSTS) {}
      CMCC->MAINT0.bit.INVALL = 1;
      CMCC->CTRL.bit.CEN = 1;
    }
  }
#endif

  __attribute__ ((long_call, noinline, section (".data#")))
  static void eraseFlash(int address, int length, int pageSize)
  {
#if defined(__SAMD51__)
    int rowSize = (pageSize * NVMCTRL->PARAM.bit.NVMP) / 64;
    for (int i = 0; i < length; i += rowSize) {
      NVMCTRL->ADDR.reg = ((uint32_t)(address + i));
      NVMCTRL->CTRLB.reg = NVMCTRL_CTRLB_CMDEX_KEY | NVMCTRL_CTRLB_CMD_EB;
      waitForReady();
      invalidate_CMCC_cache();
    }
#elif defined(ARDUINO_ARCH_SAMD)
    int rowSize = pageSize * 4;
    for (int i = 0; i < length; i += rowSize) {
      NVMCTRL->ADDR.reg = ((uint32_t)(address + i)) / 2;
      NVMCTRL->CTRLA.reg = NVMCTRL_CTRLA_CMDEX_KEY | NVMCTRL_CTRLA_CMD_ER;
      
      waitForReady();
    }
#elif defined(ARDUINO_ARCH_NRF5)
    // Enable erasing flash
    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Een << NVMC_CONFIG_WEN_Pos;

    // Erase page(s)
    int end_address = address + length;
    while (address < end_address) {
      waitForReady();
      // Erase one 1k/4k page
      NRF_NVMC->ERASEPAGE = address;
      address = address + pageSize;
    }
    // Disable erasing, enable write
    waitForReady();
    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos;
    waitForReady();
#endif    
  }

  __attribute__ ((long_call, noinline, section (".data#")))
  static void copyFlashAndReset(int dest, int src, int length, int pageSize)
  {
    volatile uint32_t* d = (volatile uint32_t*)dest;
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
  if (length > MAX_PARTIONED_SKETCH_SIZE)
    return 0;

  pageAlignedLength = ((length / PAGE_SIZE) + 1) * PAGE_SIZE; // align to page up

  _writeIndex = 0;
  _writeAddress = (uint32_t*)STORAGE_START_ADDRESS;

#if defined(__SAMD51__)
  // Enable auto dword writes
  NVMCTRL->CTRLA.bit.WMODE = NVMCTRL_CTRLA_WMODE_ADW_Val;
  waitForReady();
  // Disable NVMCTRL cache while writing, per SAMD51 errata
  NVMCTRL->CTRLA.bit.CACHEDIS0 = 1;
  NVMCTRL->CTRLA.bit.CACHEDIS1 = 1;
#elif defined(ARDUINO_ARCH_SAMD)
  // Enable auto page writes
  NVMCTRL->CTRLB.bit.MANW = 0;
#endif

  eraseFlash(STORAGE_START_ADDRESS, pageAlignedLength, PAGE_SIZE);

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

  copyFlashAndReset(SKETCH_START_ADDRESS, STORAGE_START_ADDRESS, pageAlignedLength, PAGE_SIZE);
}

long InternalStorageClass::maxSize()
{
  return MAX_PARTIONED_SKETCH_SIZE;
}

InternalStorageClass InternalStorage;

#endif
