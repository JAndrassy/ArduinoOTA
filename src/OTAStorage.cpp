#include "OTAStorage.h"

static const uint32_t pageSizes[] = { 8, 16, 32, 64, 128, 256, 512, 1024 };

#ifdef ARDUINO_SAM_ZERO
#define BOOTLOADER_SIZE        (0x4000)
#else
#define BOOTLOADER_SIZE        (0x2000)
#endif

OTAStorage::OTAStorage() :
    SKETCH_START_ADDRESS(BOOTLOADER_SIZE),
    PAGE_SIZE(pageSizes[NVMCTRL->PARAM.bit.PSZ]),
    PAGES(NVMCTRL->PARAM.bit.NVMP),
    MAX_FLASH(PAGE_SIZE * PAGES)
{

}
void ExternalOTAStorage::apply() {
  NVIC_SystemReset();
}
