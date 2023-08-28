/*
 Copyright (c) 2023 Juraj Andrassy

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

 */

#ifdef ARDUINO_ARCH_RENESAS_UNO

#include <Arduino.h>

#include "InternalStorageRenesas.h"
extern "C" {
#include "utility/r_flash_lp_cf.h"
}

static flash_lp_instance_ctrl_t flashCtrl;
static flash_cfg_t flashCfg;


InternalStorageRenesasClass::InternalStorageRenesasClass() {
  maxSketchSize = (MAX_FLASH - SKETCH_START_ADDRESS) / 2;
  maxSketchSize = (maxSketchSize / PAGE_SIZE) * PAGE_SIZE; // align to page
  storageStartAddress = SKETCH_START_ADDRESS + maxSketchSize;
  pageAlignedLength = 0;
  writeIndex = 0;
  flashWriteAddress = storageStartAddress;

  flashCfg.data_flash_bgo = false;
  flashCfg.p_callback = nullptr;
  flashCfg.p_context = nullptr;
  flashCfg.p_extend = nullptr;
  flashCfg.ipl = (BSP_IRQ_DISABLED);
  flashCfg.irq = FSP_INVALID_VECTOR;
  flashCfg.err_ipl = (BSP_IRQ_DISABLED);
  flashCfg.err_irq = FSP_INVALID_VECTOR;
}

void InternalStorageRenesasClass::debugPrint() {
  Serial.print("storageStartAddress ");
  Serial.println(storageStartAddress, HEX);
  Serial.print("SKETCH_START_ADDRESS ");
  Serial.println(SKETCH_START_ADDRESS, HEX);
  Serial.print("MAX_FLASH ");
  Serial.println(MAX_FLASH, HEX);
}

extern "C" {
  // these functions must be in RAM (.data) and NOT inlined
  // as they erase and copy the sketch data in flash

PLACE_IN_RAM_SECTION
static void copyFlashAndReset(uint32_t dest, uint32_t src, uint32_t length, uint32_t pageSize) {

  uint32_t pageCount = (length / pageSize) + 1; // align to page up
  const uint16_t buffSize = 16 * FLASH_WRITE_SIZE;
  uint8_t buff[buffSize];
  uint8_t* s = (uint8_t*) src;

  fsp_err_t rv = r_flash_lp_cf_erase(&flashCtrl, dest, pageCount, pageSize);

  while (rv == FSP_SUCCESS && length > 0) {
    for (uint16_t i = 0; i < buffSize; i++) {
      buff[i] = *s;
      s++;
    }
    rv = r_flash_lp_cf_write(&flashCtrl, (uint32_t) &buff, dest, buffSize);
    dest += buffSize;
    length -= buffSize;
  }

  NVIC_SystemReset();
}

}

int InternalStorageRenesasClass::open(int length) {

  if (length > maxSketchSize)
    return 0;

  uint32_t pageCount = (length / PAGE_SIZE) + 1; // align to page up
  pageAlignedLength = pageCount * PAGE_SIZE;
  flashWriteAddress = storageStartAddress;
  writeIndex = 0;

  fsp_err_t rv = R_FLASH_LP_Open(&flashCtrl, &flashCfg);
  if (rv != FSP_SUCCESS)
    return 0;

  __disable_irq();
  rv = r_flash_lp_cf_erase(&flashCtrl, storageStartAddress, pageCount, PAGE_SIZE);
  __enable_irq();

  return (rv == FSP_SUCCESS);
}

size_t InternalStorageRenesasClass::write(uint8_t b) {

  buffer[writeIndex] = b;
  writeIndex++;

  if (writeIndex == FLASH_WRITE_SIZE) {
    writeIndex = 0;

    __disable_irq();
    fsp_err_t rv = r_flash_lp_cf_write(&flashCtrl, (uint32_t) &buffer, flashWriteAddress, FLASH_WRITE_SIZE);
    __enable_irq();
    if (rv != FSP_SUCCESS)
      return 0;
    flashWriteAddress += FLASH_WRITE_SIZE;
  }
  return 1;
}

void InternalStorageRenesasClass::close() {
  while (writeIndex) {
    write(0xff);
  }
  R_FLASH_LP_Close(&flashCtrl);
}

void InternalStorageRenesasClass::apply() {
  fsp_err_t rv = R_FLASH_LP_Open(&flashCtrl, &flashCfg);
  if (rv != FSP_SUCCESS)
    return;
  __disable_irq();
  copyFlashAndReset(SKETCH_START_ADDRESS, storageStartAddress, pageAlignedLength, PAGE_SIZE);
}

InternalStorageRenesasClass InternalStorage;

#endif
