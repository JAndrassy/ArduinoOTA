
/*
 * The following code is based on /src/utility/ota/OTA-nano-rp2040.cpp
 * form the ArduinoIoTCloud library.
 * It is for Nano 2040 Connect with the Arduino MBED based core.
 * It works with the SFU library, which adds second stage bootloader
 * to the sketch. That bootloader checks for the update binary
 * in file system in the specific location in flash.
 * The SFU library is specific for the Nano RP2040 Connect.
 */

#include "OTAStorage.h"

#include "mbed.h"
#include "FATFileSystem.h"
#include "FlashIAPBlockDevice.h"

FlashIAPBlockDevice flash(XIP_BASE + 0xF00000, 0x100000);
mbed::FATFileSystem fs("ota");

class FileSystemStorageClass : public ExternalOTAStorage {
public:

  virtual int open(int length) {

    int err = -1;
    if ((err = flash.init()) < 0)
    {
      Serial.print(__FUNCTION__);
      Serial.print(": flash.init() failed with ");
      Serial.println(err);
      return -1;
    }

    flash.erase(XIP_BASE + 0xF00000, 0x100000);

    if ((err = fs.reformat(&flash)) != 0)
    {
      Serial.print(__FUNCTION__);
      Serial.print(": fs.reformat() failed with");
      Serial.println(err);
      return -2;
    }

    file = fopen("/ota/UPDATE.BIN", "wb");
    if (!file)
    {
      Serial.print(__FUNCTION__);
      Serial.println(": fopen() failed");
      fclose(file);
      return -3;
    }

    return 1;
  }

  virtual size_t write(uint8_t c) {
    if (fwrite(&c, 1, sizeof(c), file) != sizeof(c))
    {
      Serial.print("-");
      Serial.print(__FUNCTION__);
      Serial.println(": Writing of firmware image to flash failed");
      fclose(file);
      return 0;
    }
    return 1;
  }

  virtual void close() {
    fclose(file);
  }

  virtual void clear() {
  }

private:
  FILE * file;
};

