#include "OTAStorage.h"

#if defined(ARDUINO_ARCH_SAMD)
static const uint32_t pageSizes[] = { 8, 16, 32, 64, 128, 256, 512, 1024 };
static const uint32_t eepromSizes[] = { 16384, 8192, 4096, 2048, 1024, 512, 256, 0 };
#define EEPROM_EMULATION_RESERVATION ((*(uint32_t*)(NVMCTRL_FUSES_EEPROM_SIZE_ADDR) & NVMCTRL_FUSES_EEPROM_SIZE_Msk) >> NVMCTRL_FUSES_EEPROM_SIZE_Pos)
extern "C" {
char * __text_start__(); // 0x2000, 0x0 without bootloader and 0x4000 for M0 original bootloader
}
#elif defined(ARDUINO_ARCH_NRF5)
extern "C" {
char * __isr_vector();
}
#elif defined(__AVR__)
#include <avr/wdt.h>
#include <avr/boot.h>
#define MIN_BOOTSZ (4 * SPM_PAGESIZE)
#endif

OTAStorage::OTAStorage() :
#if defined(ARDUINO_ARCH_SAMD)
        SKETCH_START_ADDRESS((uint32_t) __text_start__),
        PAGE_SIZE(pageSizes[NVMCTRL->PARAM.bit.PSZ]),
        MAX_FLASH(PAGE_SIZE * NVMCTRL->PARAM.bit.NVMP - eepromSizes[EEPROM_EMULATION_RESERVATION])
#elif defined(ARDUINO_ARCH_NRF5)
        SKETCH_START_ADDRESS((uint32_t) __isr_vector),
        PAGE_SIZE((size_t) NRF_FICR->CODEPAGESIZE),
        MAX_FLASH(PAGE_SIZE * (uint32_t) NRF_FICR->CODESIZE)
#elif defined(__AVR__)
        SKETCH_START_ADDRESS(0),
        PAGE_SIZE(SPM_PAGESIZE),
        MAX_FLASH((uint32_t) FLASHEND + 1)
#elif defined(ESP8266) || defined(ESP32)
        SKETCH_START_ADDRESS(0), // not used
        PAGE_SIZE(0), // not used
        MAX_FLASH(0) // not used
#endif
{
  bootloaderSize = 0;
#ifdef __AVR__
  cli();
  uint8_t highBits = boot_lock_fuse_bits_get(GET_HIGH_FUSE_BITS);
  sei();
  if (!(highBits & bit(FUSE_BOOTRST))) {
    byte v = (highBits & ((~FUSE_BOOTSZ1 ) + (~FUSE_BOOTSZ0 )));
    bootloaderSize = MIN_BOOTSZ << ((v >> 1) ^ 3);
  }
#endif
}

void ExternalOTAStorage::apply() {
#ifdef __AVR__
  wdt_enable(WDTO_15MS);
  while (true);
#elif defined(ESP8266) || defined(ESP32)
  ESP.restart();
#else
  NVIC_SystemReset();
#endif
}
