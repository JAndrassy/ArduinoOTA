
# Arduino library to upload sketch over network to Arduino SAMD board

This library allows you to update sketches on your board over WiFi or Ethernet.
It requires an Arduino SAMD board like Zero, M0 or MKR.

The library is a modification of the Arduino WiFi101OTA library. For more information about how to use this library please visit
https://www.arduino.cc/en/Reference/WiFi101OTA

## Supported libraries

* Ethernet library - shields and modules with Wiznet 5100, 5200 and 5500 chips
* WiFi101 - WiFi101 shield and MKR 1000
* WiFiNINA - MKR 1010, MKR 4000, ESP32 as SPI network adapter with WiFiNINA firmware
* WiFiLink - esp8266 as network adapter with WiFiLink firmware (SPI or Serial)
* UIPEthernet - shields and modules with ENC28j60 chip
* WiFiSpi - esp8266 as SPI network adapter with WiFiSpi library updated with [this PR](https://github.com/JiriBilek/WiFiSpi/pull/12)
* WiFi - Arduino WiFi Shield (not tested)

UIPEthernet, WiFiSpi and WiFi library don't support UDP multicast for MDNS, so Arduino IDE will not show the network upload port. WiFiLink doesn't support UDP multicast, but the firmware propagates the MDNS record.

WiFiEsp library for esp8266 with AT firmware failed tests and there is no easy fix. 
