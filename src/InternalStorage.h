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

#ifndef _INTERNAL_STORAGE_H_INCLUDED
#define _INTERNAL_STORAGE_H_INCLUDED

#include "OTAStorage.h"

class InternalStorageClass : public OTAStorage {
public:

  InternalStorageClass();

  virtual int open(int length);
  virtual size_t write(uint8_t);
  virtual void close();
  virtual void clear();
  virtual void apply();
  virtual long maxSize();

  void debugPrint();

private:
  const uint32_t MAX_PARTIONED_SKETCH_SIZE, STORAGE_START_ADDRESS;

  union {
    uint32_t u32;
    uint8_t u8[4];
  } _addressData;

  uint32_t pageAlignedLength;
  int _writeIndex;
  uint32_t* _writeAddress;
};

extern InternalStorageClass InternalStorage;

#endif
