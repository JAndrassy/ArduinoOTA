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

  MAX_PARTIONED_SKETCH_SIZE((MAX_FLASH - SKETCH_START_ADDRESS) / 2),
  STORAGE_START_ADDRESS(SKETCH_START_ADDRESS + MAX_PARTIONED_SKETCH_SIZE)
{ 
  _writeIndex = 0;
  _writeAddress = nullptr;
  _sketchSize = 0;
#if defined(__SAM3X8E__)
  flash_init(FLASH_ACCESS_MODE_128, 6);
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


    
#if defined(__SAM3X8E__)
#define  us_num_pages_in_region  (IFLASH0_LOCK_REGION_SIZE / IFLASH0_PAGE_SIZE)
#define RAMFUNC __attribute__ ((section(".ramfunc")))

extern  void copyFlashAndReset(int dest, int src, int length, int pageSize);
RAMFUNC void copyFlashAndReset(int dest, int src, int length, int pageSize)

{
static uint32_t(*iap_perform_command) (uint32_t, uint32_t);
uint32_t ul_efc_nb;
/* Use IAP function with 2 parameters in ROM. */
iap_perform_command = (uint32_t(*)(uint32_t, uint32_t)) *((uint32_t *) CHIP_FLASH_IAP_ADDRESS);
ul_efc_nb = 0;



//Bank swithch forction  not working on SAM with firmware>64K - see Errata
/*
// Not actual copy, just switch banks

flash_set_gpnvm(1); //Booting from FLASH

if (FLASH_RC_YES == flash_is_gpnvm_set(2)) // BANK1 is active
{
Serial.println("Switching to BANK0");
delay(500);
noInterrupts();
//flash_clear_gpnvm(2);
iap_perform_command(ul_efc_nb,    EEFC_FCR_FKEY(0x5Au) | EEFC_FCR_FARG(2) |
				  EEFC_FCR_FCMD(EFC_FCMD_CGPB));
}

else
{
Serial.println("Switching to BANK1");
delay(500);
noInterrupts();
//flash_set_gpnvm(2);
iap_perform_command(ul_efc_nb,    EEFC_FCR_FKEY(0x5Au) | EEFC_FCR_FARG(2) |
				  EEFC_FCR_FCMD(EFC_FCMD_SGPB));
}

RSTC->RSTC_CR = 0xA5000005;
*/
// Since bank swithing is not working - just copy to bank0 from bank1
 
Efc *p_efc;
uint16_t us_page;
uint32_t *p_src;
uint32_t *p_dest;
int32_t  left;


Serial.print("Flashing bytes:");
Serial.println(length);
Serial.print("From:");
Serial.println(src,HEX);
Serial.print("To:");
Serial.println(dest,HEX);
Serial.print("PageSize:");
Serial.println(pageSize);


delay(300);
watchdogReset();


flash_set_gpnvm(1); //Booting from FLASH

// Preparing to flashing

p_efc = EFC0;
us_page = 0;

efc_set_wait_state(p_efc, 6);
p_dest = (uint32_t *) dest;
p_src = (uint32_t *) src;
left = length;

noInterrupts();

while (left>0)
    {
    // Dangerous zone - no exterlal flash based routines allowed below

                // Unlock range     
		if (! (us_page % us_num_pages_in_region))
				      iap_perform_command(ul_efc_nb,
						EEFC_FCR_FKEY(0x5Au) | EEFC_FCR_FARG(us_page) |
						EEFC_FCR_FCMD(EFC_FCMD_CLB));

		// Copy page
		for (int ul_idx = 0; ul_idx < (IFLASH0_PAGE_SIZE / sizeof(uint32_t)); ++ul_idx) 
		                          {
					    *p_dest++ = *p_src++;
					  }

               
                if ((us_page+1) % us_num_pages_in_region)
	 
		// Erase & write page		
		    iap_perform_command(ul_efc_nb,
			EEFC_FCR_FKEY(0x5Au) | EEFC_FCR_FARG(us_page) |
			EEFC_FCR_FCMD(EFC_FCMD_EWP));
			//	result at  (p_efc->EEFC_FSR & EEFC_ERROR_FLAGS);
		
		else // last page in the lock region
		// Erase, write, lock	page		
		   iap_perform_command(ul_efc_nb,
			EEFC_FCR_FKEY(0x5Au) | EEFC_FCR_FARG(us_page) |
			EEFC_FCR_FCMD(EFC_FCMD_EWPL));
			//	result at  (p_efc->EEFC_FSR & EEFC_ERROR_FLAGS);




    us_page++;
    left -= IFLASH0_PAGE_SIZE;
    //watchdogReset();

    }
// End low-level flashing code    
    


//Restart
RSTC->RSTC_CR = 0xA5000005;

}


#else

  __attribute__ ((long_call, noinline, section (".data#")))
  static void copyFlashAndReset(int dest, int src, int length, int pageSize)
  {
    uint32_t* d = (uint32_t*)dest;
    uint32_t* s = (uint32_t*)src;

  // disable interrupts, as vector table will be erase during flash sequence    
    noInterrupts();

    eraseFlash(dest, length, pageSize);

    for (int i = 0; i < length; i += 4) {
      *d++ = *s++;
    
      waitForReady();
    }

    NVIC_SystemReset();
}

#endif

} //extern C


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

 int retCode = flash_unlock((uint32_t)STORAGE_START_ADDRESS, (uint32_t) STORAGE_START_ADDRESS + MAX_PARTIONED_SKETCH_SIZE - 1, 0, 0);
  if (retCode != FLASH_RC_OK) {
    Serial.println("Failed to unlock flash for write\n");
    }
#endif

eraseFlash(STORAGE_START_ADDRESS, MAX_PARTIONED_SKETCH_SIZE, PAGE_SIZE);

  return 1;
}



size_t InternalStorageClass::write(uint8_t b)
{
 
#if defined(__SAM3X8E__)
     _addressData[_writeIndex] = b;
     _writeIndex++;

  if ( _writeIndex == IFLASH0_PAGE_SIZE ) {
    _writeIndex = 0;
    
    watchdogReset();
    Serial.print("=");
    int retCode = flash_write((uint32_t) _writeAddress, &_addressData, IFLASH0_PAGE_SIZE, 0); //1?
    if (retCode != FLASH_RC_OK) 
		{
		Serial.println("Flash write failed\n");
                return 0;
                }
    _writeAddress++;
   }
#else
     _addressData.u8[_writeIndex] = b;
     _writeIndex++;

  if (_writeIndex == sizeof(addressData)) {
	    _writeIndex = 0;
	    _writeAddress->u32 = _addressData.u32;
	    _writeAddress++;
	    waitForReady();
    }
#endif

return 1;
}

void InternalStorageClass::close()  
{
#if defined(__SAM3X8E__)
_sketchSize = (uint32_t)_writeAddress-STORAGE_START_ADDRESS+_writeIndex;

while (_writeIndex) write(0xff);  //Finish block

Serial.print("\nReceived bytes:");
Serial.println(_sketchSize);
#else 

  while ((int)_writeAddress % PAGE_SIZE) {
    write(0xff); 
  }    
#endif  
  
}

void InternalStorageClass::clear()
{
}


void InternalStorageClass::apply()
{
  //noInterrupts(); moved inside routine

#if defined(__SAM3X8E__)
  copyFlashAndReset(SKETCH_START_ADDRESS, STORAGE_START_ADDRESS, _sketchSize, PAGE_SIZE);
#else
  copyFlashAndReset(SKETCH_START_ADDRESS, STORAGE_START_ADDRESS, MAX_PARTIONED_SKETCH_SIZE, PAGE_SIZE);
#endif

}

long InternalStorageClass::maxSize()
{
  return MAX_PARTIONED_SKETCH_SIZE;
}

InternalStorageClass InternalStorage;

#endif
