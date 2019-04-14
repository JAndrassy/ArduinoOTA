
# Arduino library to upload sketch over network to supported Arduino board

This library allows you to update sketches on your board over WiFi or Ethernet.

The library is a modification of the Arduino WiFi101OTA library.

![network port in IDE](ArduinoOTA.png)

## Supported micro-controllers

* ATmega AVR with at least 64 kB of flash (Arduino Mega, [MegaCore](https://github.com/MCUdude/MegaCore) MCUs, MightyCore 1284p and 644)
* Arduino SAMD boards like Zero, M0 or MKR 
* nRF5 board supported by [nRF5 core](https://github.com/sandeepmistry/arduino-nRF5).
* boards supported by ESP8266 and ESP32 Arduino boards package  

## Supported networking libraries

* Ethernet library - shields and modules with Wiznet 5100, 5200 and 5500 chips
* WiFi101 - WiFi101 shield and MKR 1000
* WiFiNINA - MKR 1010, MKR 4000, ESP32 as SPI network adapter with WiFiNINA firmware
* WiFiLink - esp8266 as network adapter with WiFiLink firmware (SPI or Serial)
* UIPEthernet - shields and modules with ENC28j60 chip
* WiFiSpi - esp8266 as SPI network adapter with WiFiSpi library updated with [this PR](https://github.com/JiriBilek/WiFiSpi/pull/12)
* WiFi - Arduino WiFi Shield (not tested)
* WiFi library of ESP8266 and ESP32 Arduino boards package

UIPEthernet, WiFiSpi and WiFi library don't support UDP multicast for MDNS, so Arduino IDE will not show the network upload port. WiFiLink doesn't support UDP multicast, but the firmware propagates the MDNS record.

WiFiEsp library for esp8266 with AT firmware failed tests and there is no easy fix. 

## Installation

The library is in Library Manager of the Arduino IDE.

Arduino SAMD boards (Zero, M0, MKR) are supported 'out of the box'.

For nRF5 boards two lines need to be added to platform.txt file of the nRF5 Arduino package (until the PR that adds them is accepted and included in a release). Only nRF51 was tested until now. For details scroll down.

For ESP8266 and ESP32 boards, platform.local.txt from extras folder need to be copied into boards package installation folder and the bundled ArduinoOTA library must be deleted. For details scroll down.

ATmega boards require to flash a modified Optiboot bootloader for flash write operations. Details are below.

## ATmega support

The size of networking library and SD library limit the use of ArduinoOTA library to ATmega MCUs with at least 64 kB flash memory. 

*There are other network upload options for here excluded ATmega328p: ([Ariadne bootloader](https://github.com/loathingKernel/ariadne-bootloader) for Wiznet chips, [WiFiLink firmware](https://github.com/jandrassy/arduino-firmware-wifilink) for the esp8266).*

For upload over InternalStorage, Optiboot bootloader with [`copy_flash_pages` function](https://github.com/Optiboot/optiboot/pull/269) is required. Arduino AVR package doesn't use Optiboot for Arduino Mega. For Arduino Mega you can download [my boards definitions](https://github.com/jandrassy/my_boards) and use it [to burn](https://arduino.stackexchange.com/questions/473/how-do-i-burn-the-bootloader) the modified Optiboot and to upload sketches to your Mega over USB and over network. 

For SDStorage a 'SD bootloader' is required to load the uploaded file from the SD card. There is no good SD bootloader. 2boots works only with not available old types of SD cards and zevero/avr_boot doesn't yet support USB upload of sketch. The SDStorage was tested with zevero/avr_boot. The ATmega_SD example shows how to use this ArduinoOTA library with SD bootloader.

To use remote upload from IDE with SDStorage or InternalStorage, copy platform.local.txt from extras/avr folder, next to platform.txt in the boards package used (Arduino-avr or MCUdude packages). Packages are located in ~/.arduino15/packages/ on Linux and %userprofile%\AppData\Local\Arduino15\packages\ on Windows (AppData is a hidden folder). The platform.local.txt contains a line to create a .bin file and a line to override tools.avrdude.upload.network_pattern, because in platform.txt it is defined for Yun. 

The IDE upload tool is installed with Arduino AVR core package. At least version 1.2 of the arduinoOTA tool is required. For upload from command line without IDE see the command template in extras/avr/platform.local.txt.

## ESP8266 and ESP32 support

The ArduinoOTA library bundled with ESP8266 and ESP32 Arduino packages works only with native WiFi libraries. This library allows to upload a sketch to esp8266 or esp32 over Ethernet with Ethernet or UIPEthernet library. Upload over the native WiFi library works too.

To use this library instead of the bundled library, the bundled library must be removed from the boards package library folder. To override the configuration of OTA upload in platform.txt, copy the platform.local.txt file from extras folder of this library next to platform.txt file in boards package installation folder. Packages are located in ~/.arduino15/packages/ on Linux and %userprofile%\AppData\Local\Arduino15\packages\ on Windows (AppData is a hidden folder).

The esp8266 boards package has bundled Ethernet library. It is an old version of the Arduino Ethernet library which works only with W5100 chips. It is better to remove it from boards package and install the Ethernet library from Library Manager in IDE.

This library supports SPIFFS upload to esp8266 and esp32, but the IDE plugins have the network upload tool hardcoded to espota. It can't be changed in configuration. To upload SPIFFS, call the plugin in Tools menu and after it fails to upload over network, go to location of the created bin file and upload the file with arduinoOTA tool from command line. The location of the file is printed in the IDE console window. Upload command example (linux):
```
~/arduino-1.8.8/hardware/tools/avr/bin/arduinoOTA -address 192.168.1.107 -port 65280 -username arduino -password password -sketch OTEthernet.spiffs.bin -upload /data -b
```
(the same command can be used to upload the sketch binary, only use `-upload /sketch`)

## nRF5 support

Note: Only nRF51 was tested for now

If SoftDevice is not used, the sketch is written from address 0. For write to address 0 the sketch must be compiled with -fno-delete-null-pointer-checks.

For SD card update use [SDUnRF5 library](https://github.com/jandrassy/SDUnRF5).

To use remote upload from IDE, add lines from [this PR](https://github.com/sandeepmistry/arduino-nRF5/pull/327/commits/4b70ae7207124bd92afa14e562e4f0c4d931220d) to sandeepmistry/hardware/nRF5/0.6.0/platform.txt at the end of section "OpenOCD sketch upload":

If you use SoftDevice, stop BLE before applying update. Use `ArduinoOTA.beforeApply` to register a callback function. For example in setup `ArduinoOTA.beforeApply(shutdown);` and add the function to to sketch:

```
void shutdown() {
  blePeripheral.end();
}
```
## Boards tested

* SAMD
    - Arduino MKR Zero
    - Crowduino M0 SD
* nRF5
    - Seeed Arch Link
* ATmega
    - Arduino Mega
    - Badio 1284p
* esp8266
    - Wemos D1 mini
* esp32
    - ESP32 Dev Module    

## Contribution

Please report tested boards.

Other ARM based MCUs could be added with code similar to SAMD and nRF5. 
