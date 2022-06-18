#include "OTAStorage.h"

#if defined(ARDUINO_ARCH_SAMD)
static const uint32_t pageSizes[] = { 8, 16, 32, 64, 128, 256, 512, 1024 };
static const uint32_t eepromSizes[] = { 16384, 8192, 4096, 2048, 1024, 512, 256, 0 };
#if defined(__SAMD51__)
#  define EEPROM_EMULATION_RESERVATION 0
#else
#  define EEPROM_EMULATION_RESERVATION ((*(uint32_t*)(NVMCTRL_FUSES_EEPROM_SIZE_ADDR) & NVMCTRL_FUSES_EEPROM_SIZE_Msk) >> NVMCTRL_FUSES_EEPROM_SIZE_Pos)
#endif
extern "C" {
char * __text_start__(); // 0x2000, 0x0 without bootloader and 0x4000 for M0 original bootloader
}
#elif defined(ARDUINO_ARCH_NRF5)
extern "C" {
char * __isr_vector();
}
#elif defined(ARDUINO_ARCH_STM32)
#include "stm32yyxx_ll_utils.h"
extern "C" char * g_pfnVectors; // at first address of the binary. 0x0000 without bootloader. 0x2000 with Maple bootloader
#elif defined(ARDUINO_ARCH_RP2040)
#include <hardware/flash.h>
extern "C" uint8_t _FS_start;
extern "C" uint8_t _EEPROM_start;
#elif defined(ARDUINO_ARCH_MEGAAVR)
#include <avr/wdt.h>
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
#elif defined(ARDUINO_ARCH_STM32)
        SKETCH_START_ADDRESS((uint32_t) &g_pfnVectors - FLASH_BASE), // start address depends on bootloader size
#ifdef FLASH_PAGE_SIZE
        PAGE_SIZE(FLASH_PAGE_SIZE),
#else
        PAGE_SIZE(4), //for FLASH_TYPEPROGRAM_WORD
#endif
        MAX_FLASH((min(LL_GetFlashSize(), 512ul) * 1024)) // LL_GetFlashSize returns size in kB. 512 is max for a bank
#elif defined(ARDUINO_ARCH_RP2040)
        SKETCH_START_ADDRESS(0),
        PAGE_SIZE(FLASH_PAGE_SIZE),
        MAX_FLASH((uint32_t) min(&_FS_start, &_EEPROM_start) - XIP_BASE)
#elif defined(__AVR__) && !defined(ARDUINO_ARCH_MEGAAVR)
        SKETCH_START_ADDRESS(0),
        PAGE_SIZE(SPM_PAGESIZE),
        MAX_FLASH((uint32_t) FLASHEND + 1)
#else
        SKETCH_START_ADDRESS(0), // not used
        PAGE_SIZE(0), // not used
        MAX_FLASH(0) // not used
#endif
{
  bootloaderSize = 0;
#if defined(__AVR__) && !defined(ARDUINO_ARCH_MEGAAVR)
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
#if defined(ARDUINO_ARCH_MEGAAVR)
  wdt_enable(WDT_PERIOD_8CLK_gc);
  while (true);
#elif defined(__AVR__)
  wdt_enable(WDTO_15MS);
  while (true);
#elif defined(ESP8266) || defined(ESP32)
  ESP.restart();
#else
  NVIC_SystemReset();
#endif
}
