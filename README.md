
# Arduino library to upload sketch over network to Arduino board (SAMD, nRF5)

This library allows you to update sketches on your board over WiFi or Ethernet.
It requires an Arduino SAMD board like Zero, M0 or MKR or a nRF5 board supported by nRF5 core.

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

## nRF5 support

Note: Only nRF51 was tested for now

If SoftDevice is not used, the sketch is written from address 0. For write to address 0 the sketch must be compiled with -fno-delete-null-pointer-checks.

For SD card update use [SDUnRF5 library](https://github.com/jandrassy/SDUnRF5).

To use remote upload from IDE, add following lines to sandeepmistry/hardware/nRF5/0.6.0/platform.txt at the end of section "OpenOCD sketch upload":

```
tools.openocd.network_cmd={runtime.tools.arduinoOTA.path}/bin/arduinoOTA
tools.openocd.upload.network_pattern="{network_cmd}" -address {serial.port} -port 65280 -username arduino -password "{network.password}" -sketch "{build.path}/{build.project_name}.bin" -upload /sketch -b
```

If you use SoftDevice, stop BLE before applying update. Use `OTEthernet.beforeApply` to register a callbak function. For example in setup `OTEthernet.beforeApply(shutdown);` and add the function to to sketch:

```
void shutdown() {
  blePeripheral.end();
}
```

