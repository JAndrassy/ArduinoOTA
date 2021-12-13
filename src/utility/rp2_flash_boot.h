#ifndef _RP2_FLASH_BOOT_H
#define _RP2_FLASH_BOOT_H

#include <pico.h>

#ifdef __cplusplus
extern "C" {
#endif

void copy_flash_pages(uint32_t flash_offs, const uint8_t *data, size_t count, bool reset);

#ifdef __cplusplus
}
#endif

#endif
