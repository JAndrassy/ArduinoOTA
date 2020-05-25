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
// Not used
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

    //Erase secondary bank;
    Serial.println("Erase flash");
    flash_erase_all(address); 
#endif    
  }

  __attribute__ ((long_call, noinline, section (".data#")))
  static void copyFlashAndReset(int dest, int src, int length, int pageSize)
  {

#if defined(__SAM3X8E__)
// Not actual copy, just switch banks
flash_set_gpnvm(1); //Booting from FLASH

/*

HMM not working - starting & hung
if (FLASH_RC_YES == flash_is_gpnvm_set(2)) // BANK1 is active
{
Serial.println("Switching to BANK0");
delay(500);
noInterrupts();
flash_clear_gpnvm(2);
}

else
{
Serial.println("Switching to BANK1");
delay(500);
noInterrupts();
flash_set_gpnvm(2);
}
*/

//TODO - less bytes
Serial.print("Copying bytes:");
Serial.println(length);
delay(300);

noInterrupts();

//NOt working
//eraseFlash(dest, length, pageSize);
//flash_erase_all(dest); 

while (length>=256)
    {
    flash_write(dest,(void*) src,256,1);
    dest+=256;
    src+=256;
    length-=256;
    watchdogReset();
    }
flash_write(dest,(void*) src,length,1);

// TODO - lock flash
//Todo - clean STORAGE (not working)
//flash_erase_all(src); 

RSTC->RSTC_CR = 0xA5000005;

#else
    uint32_t* d = (uint32_t*)dest;
    uint32_t* s = (uint32_t*)src;
    
    noInterrupts();

    eraseFlash(dest, length, pageSize);

    for (int i = 0; i < length; i += 4) {
      *d++ = *s++;
    
      waitForReady();
    }
#endif
    NVIC_SystemReset();
  }
}

int InternalStorageClass::open(int length)
{
  (void)length;
  _writeIndex = 0;
  _writeAddress = (addressData*)STORAGE_START_ADDRESS;

Serial.println("Open flash for write\n");

#ifdef ARDUINO_ARCH_SAMD
  // enable auto page writes
  NVMCTRL->CTRLB.bit.MANW = 0;
#endif

#if defined(__SAM3X8E__)
//TODO - remove
 int retCode = flash_unlock((uint32_t)STORAGE_START_ADDRESS, (uint32_t) STORAGE_START_ADDRESS + MAX_PARTIONED_SKETCH_SIZE - 1, 0, 0);
  if (retCode != FLASH_RC_OK) {
    Serial.println("Failed to unlock flash for write\n");
    }
#endif

eraseFlash(STORAGE_START_ADDRESS, MAX_PARTIONED_SKETCH_SIZE, PAGE_SIZE);

  return 1;
}




/*

uint32_t flash_write(uint32_t ul_address, const void *p_buffer,
		uint32_t ul_size, uint32_t ul_erase_flag)
{
	Efc *p_efc;
	uint32_t ul_fws_temp;
	uint16_t us_page;
	uint16_t us_offset;
	uint32_t writeSize;
	uint32_t ul_page_addr;
	uint16_t us_padding;
	uint32_t ul_error;
	uint32_t ul_idx;
	uint32_t *p_aligned_dest;
	uint8_t *puc_page_buffer = (uint8_t *) gs_ul_page_buffer;

	translate_address(&p_efc, ul_address, &us_page, &us_offset);

	/// According to the errata, set the wait state value to 6. 
	ul_fws_temp = efc_get_wait_state(p_efc);
	efc_set_wait_state(p_efc, 6);

	// Write all pages 
	while (ul_size > 0) {
		// Copy data in temporary buffer to avoid alignment problems. 
		writeSize = Min((uint32_t) IFLASH_PAGE_SIZE - us_offset,
				ul_size);
		compute_address(p_efc, us_page, 0, &ul_page_addr);
		us_padding = IFLASH_PAGE_SIZE - us_offset - writeSize;

		// Pre-buffer data 
		memcpy(puc_page_buffer, (void *)ul_page_addr, us_offset);

		// Buffer data 
		memcpy(puc_page_buffer + us_offset, p_buffer, writeSize);

		// Post-buffer data 
		memcpy(puc_page_buffer + us_offset + 
		writeSize,
				(void *)(ul_page_addr + us_offset + writeSize),
				us_padding);

		// Write page.
		//  Writing 8-bit and 16-bit data is not allowed and may lead to
		// unpredictable data corruption.
		//
		p_aligned_dest = (uint32_t *) ul_page_addr;
		for (ul_idx = 0; ul_idx < (IFLASH_PAGE_SIZE / sizeof(uint32_t));
				++ul_idx) {
			*p_aligned_dest++ = gs_ul_page_buffer[ul_idx];
		}

		if (ul_erase_flag) {
			ul_error = efc_perform_command(p_efc, EFC_FCMD_EWP,
					us_page);
		} else {
			ul_error = efc_perform_command(p_efc, EFC_FCMD_WP,
					us_page);
		}

		if (ul_error) {
			return ul_error;
		}

		// Progression 
		p_buffer = (void *)((uint32_t) p_buffer + writeSize);
		ul_size -= writeSize;
		us_page++;
		us_offset = 0;
	}

	// According to the errata, restore the wait state value. 
	efc_set_wait_state(p_efc, ul_fws_temp);

	return FLASH_RC_OK;
}

*/


size_t InternalStorageClass::write(uint8_t b)
{
  _addressData.u8[_writeIndex] = b;
  _writeIndex++;

  if (_writeIndex == sizeof(addressData)) {
    _writeIndex = 0;

#if defined(__SAM3X8E__)
    watchdogReset();
    Serial.print("=");
    int retCode = flash_write((uint32_t) _writeAddress, &_addressData.u32, sizeof(addressData), 0);
    if (retCode != FLASH_RC_OK) 
		{
		Serial.println("Flash write failed\n");
                return 0;
                }
    _writeAddress++;

#else
    _writeAddress->u32 = _addressData.u32;
    _writeAddress++;
    waitForReady();
#endif

  }

  return 1;
}

void InternalStorageClass::close()
{
  while ((int)_writeAddress % PAGE_SIZE) {
    write(0xff); 
  }    
#if defined(__SAM3X8E__)
Serial.println("\nClosing flash");
Serial.println((uint32_t)_writeAddress-STORAGE_START_ADDRESS+_writeIndex);

  while (_writeIndex) {
			write(0xff);
			}  

//TODO move lock up
  int retCode = flash_lock((uint32_t)STORAGE_START_ADDRESS, (uint32_t) STORAGE_START_ADDRESS+MAX_PARTIONED_SKETCH_SIZE-1, 0, 0);
  if (retCode != FLASH_RC_OK) 
    Serial.println("Failed to lock flash for write\n");
#endif  
  
}

void InternalStorageClass::clear()
{
}


void InternalStorageClass::apply()
{
  // disable interrupts, as vector table will be erase during flash sequence
  //noInterrupts();
  copyFlashAndReset(SKETCH_START_ADDRESS, STORAGE_START_ADDRESS, MAX_PARTIONED_SKETCH_SIZE, PAGE_SIZE);
}

long InternalStorageClass::maxSize()
{
  return MAX_PARTIONED_SKETCH_SIZE;
}

InternalStorageClass InternalStorage;

#endif
