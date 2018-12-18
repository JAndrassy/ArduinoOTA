#include "OTAStorage.h"

#if defined(ARDUINO_ARCH_SAMD)
static const uint32_t pageSizes[] = { 8, 16, 32, 64, 128, 256, 512, 1024 };
#ifdef ARDUINO_SAM_ZERO
#define BOOTLOADER_SIZE        (0x4000)
#else
#define BOOTLOADER_SIZE        (0x2000)
#endif
#elif defined(ARDUINO_ARCH_NRF5)
extern "C" {
char * __isr_vector();
}
#endif


OTAStorage::OTAStorage() :
#if defined(ARDUINO_ARCH_SAMD)
        SKETCH_START_ADDRESS(BOOTLOADER_SIZE),
        PAGE_SIZE(pageSizes[NVMCTRL->PARAM.bit.PSZ]),
        PAGES(NVMCTRL->PARAM.bit.NVMP),
#elif defined(ARDUINO_ARCH_NRF5)
        SKETCH_START_ADDRESS((uint32_t) __isr_vector),
        PAGE_SIZE((size_t) NRF_FICR->CODEPAGESIZE),
        PAGES((uint32_t) NRF_FICR->CODESIZE),
#endif
        MAX_FLASH(PAGE_SIZE * PAGES)
{

}
void ExternalOTAStorage::apply() {
  NVIC_SystemReset();
}
