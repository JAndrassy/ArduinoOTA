/*
 This example reads HEX file from SD card and writes it
 to InternalStorage to store and apply it as update.

 To read Intel HEX file format ihex library
 by Kimmo Kulovesi is used as source files.
 https://github.com/arkku/ihex

 Created for ArduinoOTA library in May 2022
 by Juraj Andrassy
 */

#include <ArduinoOTA.h> // include before SD.h (to not intialize SDStorage)
#include <SD.h>
#include "kk_ihex_read.h"

const int SDCARD_CS = 4;

int ihex2binError = 0;

void setup() {
  Serial.begin(115200);

  Serial.print("\nInitializing SD card...");
  if (!SD.begin(SDCARD_CS)) {
    Serial.println(F("SD card initialization failed!"));
    // don't continue
    while (true);
  }
  Serial.println();

  File hexFile = SD.open("UPDATE.HEX");
  if (hexFile) {
    Serial.println("Update HEX file found. Performing update...");

    ihex_state ihex;
    ihex_begin_read(&ihex);
    InternalStorage.open(0); // InternalStorageAVR doesn't use the length parameter

    char buffer[64];
    int lineNumber = 0;
    while (hexFile.available() && !ihex2binError) {
      int length = hexFile.readBytesUntil('\n', buffer, sizeof(buffer));
      lineNumber++;
      ihex_read_bytes(&ihex, buffer, length);
    }
    ihex_end_read(&ihex);
    hexFile.close();
    SD.remove("UPDATE.HEX");

    switch (ihex2binError) {
      case 0: // no error
        Serial.println("apply and reset...");
        Serial.flush();
        InternalStorage.apply();
        break;
      case 1:
        Serial.print("Checksum error on line ");
        Serial.println(lineNumber);
        break;
      case 2:
        Serial.print("Line length error on line  ");
        Serial.println(lineNumber);
        break;
      case 3:
        Serial.print("Not continues address on line ");
        Serial.println(lineNumber);
        break;
      case 4:
        Serial.print("Sketch is larger than half of the flash.");
        break;
    }
  } else {
    Serial.println("Update HEX not present.");
  }
}

void loop() {

}

ihex_bool_t ihex_data_read(ihex_state *ihex, ihex_record_type_t type, ihex_bool_t checkSumError) {

  static uint32_t bytesWritten = 0;

  if (checkSumError) {
    ihex2binError = 1;
  } else if (ihex->length < ihex->line_length) {
    ihex2binError = 2;
  } else if (type == IHEX_DATA_RECORD) {
    if (ihex->address != bytesWritten) {
      ihex2binError = 3;
    } else if (ihex->address > InternalStorage.maxSize()) {
      ihex2binError = 4;
    } else {
      for (int i = 0; i < ihex->length; i++) {
        InternalStorage.write(ihex->data[i]);
        bytesWritten++;
      }
    }
  } else if (type == IHEX_END_OF_FILE_RECORD) {
    InternalStorage.close();
    Serial.print(bytesWritten);
    Serial.println(" bytes written to flash");
  }
  return true;
}
