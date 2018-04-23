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

#ifndef _SERIALFLASH_STORAGE_H_INCLUDED
#define _SERIALFLASH_STORAGE_H_INCLUDED

#include <SerialFlash.h>

#include "OTAStorage.h"

#define SERIAL_FLASH_BUFFER_SIZE    64
#define SERIAL_FLASH_CS             5

class SerialFlashStorageClass : public OTAStorage {
public:
  virtual int open(int length);
  virtual size_t write(uint8_t);
  virtual void close();
  virtual void clear();
  virtual void apply();

private:
  SerialFlashFile _file;
};

extern SerialFlashStorageClass SerialFlashStorage;

#endif
