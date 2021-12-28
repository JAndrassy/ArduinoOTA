/*
  Copyright (c) 2021 Juraj Andrassy

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

#ifndef _INTERNAL_STORAGE_RP2_H_INCLUDED
#define _INTERNAL_STORAGE_RP2_H_INCLUDED

#include "OTAStorage.h"

class InternalStorageSTM32Class : public OTAStorage {
public:

  InternalStorageSTM32Class();

  virtual int open(int length);
  virtual size_t write(uint8_t);
  virtual void close();
  virtual void clear() {}
  virtual void apply();
  virtual long maxSize() {return maxSketchSize;}

private:
  union {
    uint64_t u64;
    uint8_t u8[8];
  } addressData;

  uint32_t maxSketchSize;
  uint32_t storageStartAddress;
  uint32_t pageAlignedLength;
  uint8_t writeIndex;
  uint32_t flashWriteAddress;
};

extern InternalStorageSTM32Class InternalStorage;

#endif
