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
*/

#include "SerialFlashStorage.h"

#define UPDATE_FILE "UPDATE.BIN"

int SerialFlashStorageClass::open(int contentLength)
{
  if (!SerialFlash.begin(SERIAL_FLASH_CS)) {
    return 0;
  }

  while (!SerialFlash.ready()) {}

  if (SerialFlash.exists(UPDATE_FILE)) {
    SerialFlash.remove(UPDATE_FILE);
  }

  if (SerialFlash.create(UPDATE_FILE, contentLength)) {
    _file = SerialFlash.open(UPDATE_FILE);
  }

  if (!_file) {
    return 0;
  }

  return 1;
}

size_t SerialFlashStorageClass::write(uint8_t b)
{
  while (!SerialFlash.ready()) {}
  int ret = _file.write(&b, 1);
  return ret;
}

void SerialFlashStorageClass::close()
{
  _file.close();
}

void SerialFlashStorageClass::clear()
{
  SerialFlash.remove(UPDATE_FILE);
}

void SerialFlashStorageClass::apply()
{
  // just reset, SDU copies the data to flash
  NVIC_SystemReset();
}

SerialFlashStorageClass SerialFlashStorage;
