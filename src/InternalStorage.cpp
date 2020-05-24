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

#if !defined(__AVR__) && !defined(ESP8266) && !defined(ESP32)

#include <Arduino.h>

#include "InternalStorage.h"

#if defined(__SAM3X8E__)
#include "flash_efc.h"
#include "efc.h"
#endif


InternalStorageClass::InternalStorageClass() :

#if defined(__SAM3X8E__)
  MAX_PARTIONED_SKETCH_SIZE(256*1024),
  STORAGE_START_ADDRESS(IFLASH1_ADDR)
#else
  MAX_PARTIONED_SKETCH_SIZE((MAX_FLASH - SKETCH_START_ADDRESS) / 2),
  STORAGE_START_ADDRESS(SKETCH_START_ADDRESS + MAX_PARTIONED_SKETCH_SIZE)
#endif
{
  _writeIndex = 0;
  _writeAddress = nullptr;

#if defined(__SAM3X8E__)
  flash_init(FLASH_ACCESS_MODE_128, 6);
  //if (retCode != FLASH_RC_OK)
#endif
}

extern "C" {
  // these functions must be in RAM (.data) and NOT inlined
  // as they erase and copy the sketch data in flash

  __attribute__ ((long_call, noinline, section (".data#"))) //
  void waitForReady() {
#if defined(ARDUINO_ARCH_SAMD)
    while (!NVMCTRL->INTFLAG.bit.READY);
#elif defined(ARDUINO_ARCH_NRF5)
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy);
#elif defined(__SAM3X8E__)
/////////// while ...    
#endif
  }

  __attribute__ ((long_call, noinline, section (".data#")))
  static void eraseFlash(int address, int length, int pageSize)
  {
#if defined(ARDUINO_ARCH_SAMD)
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
#elif defined(__SAM3X8E__)
/////////////////    
#endif    
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

Serial.println("Open flash for write\n");

#ifdef ARDUINO_ARCH_SAMD
  // enable auto page writes
  NVMCTRL->CTRLB.bit.MANW = 0;
#endif

#if defined(__SAM3X8E__)
 int retCode = flash_unlock((uint32_t)STORAGE_START_ADDRESS, (uint32_t) MAX_PARTIONED_SKETCH_SIZE, 0, 0);
  if (retCode != FLASH_RC_OK) {
    Serial.println("Failed to unlock flash for write\n");
    }
#else
  eraseFlash(STORAGE_START_ADDRESS, MAX_PARTIONED_SKETCH_SIZE, PAGE_SIZE);
#endif



  return 1;
}

size_t InternalStorageClass::write(uint8_t b)
{
  _addressData.u8[_writeIndex] = b;
  _writeIndex++;

  if (_writeIndex == 4) {
    _writeIndex = 0;

#if defined(__SAM3X8E__)
    int retCode = flash_write((uint32_t) _writeAddress, &_addressData.u32, 4, 1);
    if (retCode != FLASH_RC_OK) 
		Serial.println("Flash write failed\n");

    _writeAddress++;

#else
    *_writeAddress = _addressData.u32;
    _writeAddress++;
    waitForReady();
#endif

  }

  return 1;
}

void InternalStorageClass::close()
{
Serial.println("Closing flash\n");

  while ((int)_writeAddress % PAGE_SIZE) {
    write(0xff);
  }  
#if defined(__SAM3X8E__)
  int retCode = flash_lock((uint32_t)STORAGE_START_ADDRESS, (uint32_t) MAX_PARTIONED_SKETCH_SIZE, 0, 0);
  if (retCode != FLASH_RC_OK) 
    Serial.println("Failed to lock flash for write\n");
#endif  
  
}

void InternalStorageClass::clear()
{
}


void InternalStorageClass::apply()
{
#if defined(__SAM3X8E__)
if (flash_is_gpnvm_set(2)) // BANK1 is active
{
Serial.println("Switching to BANK0");
flash_clear_gpnvm(2);

}

else
{
Serial.println("Switching to BANK1");
flash_set_gpnvm(2);
}

flash_set_gpnvm(1); //Booting from FLASH

Serial.println("Restarting");
delay(500);

RSTC->RSTC_CR = 0xA5000005;
#else
  // disable interrupts, as vector table will be erase during flash sequence
  noInterrupts();

  copyFlashAndReset(SKETCH_START_ADDRESS, STORAGE_START_ADDRESS, MAX_PARTIONED_SKETCH_SIZE, PAGE_SIZE);
#endif
}

long InternalStorageClass::maxSize()
{
  return MAX_PARTIONED_SKETCH_SIZE;
}

InternalStorageClass InternalStorage;

#endif
