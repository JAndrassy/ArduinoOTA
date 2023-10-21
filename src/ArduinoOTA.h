/*
  Copyright (c) 2018 Juraj Andrassy

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

#ifndef _ARDUINOOTA_H_
#define _ARDUINOOTA_H_

#include "WiFiOTA.h"

#ifdef __AVR__
#if FLASHEND >= 0xFFFF
#include "InternalStorageAVR.h"
#endif
#elif defined(ARDUINO_ARCH_STM32)
#include <InternalStorageSTM32.h>
#elif defined(ARDUINO_ARCH_RP2040)
#include <InternalStorageRP2.h>
#elif defined(ARDUINO_ARCH_RENESAS_UNO)
#include <InternalStorageRenesas.h>
#elif defined(ESP8266) || defined(ESP32)
#include "InternalStorageESP.h"
#else
#include "InternalStorage.h"
#endif
#ifdef __SD_H__
#include "SDStorage.h"
SDStorageClass SDStorage;
#endif
#ifdef SerialFlash_h_
#include "SerialFlashStorage.h"
SerialFlashStorageClass SerialFlashStorage;
#endif

const uint16_t OTA_PORT = 65280;

template <class NetServer, class NetClient>
class ArduinoOTAClass : public WiFiOTAClass {

private:
  NetServer server;

public:
  ArduinoOTAClass() : server(OTA_PORT) {};

  void begin(IPAddress localIP, const char* name, const char* password, OTAStorage& storage) {
    WiFiOTAClass::begin(localIP, name, password, storage);
    server.begin();
  }

  void end() {
#if defined(_WIFI_ESP_AT_H_)|| defined(WiFiS3_h) || defined(ESP32) || defined(UIPETHERNET_H)
    server.end();
#elif defined(ESP8266) || (defined(ARDUINO_ARCH_RP2040) && !defined(ARDUINO_ARCH_MBED))
    server.stop();
#else
//#warning "The networking library doesn't have a function to stop the server"
#endif
  }

  void poll() {
    NetClient client = server.available();
    pollServer(client);
  }

  void handle() { // alias
    poll();
  }

};

template <class NetServer, class NetClient, class NetUDP>
class ArduinoOTAMdnsClass : public ArduinoOTAClass<NetServer, NetClient> {

private:
  NetUDP mdnsSocket;

public:
  ArduinoOTAMdnsClass() {};

  void begin(IPAddress localIP, const char* name, const char* password, OTAStorage& storage) {
    ArduinoOTAClass<NetServer, NetClient>::begin(localIP, name, password, storage);
#if (defined(ESP8266) || defined(ARDUINO_RASPBERRY_PI_PICO_W)) && !(defined(ethernet_h_) || defined(ethernet_h) || defined(UIPETHERNET_H))
    mdnsSocket.beginMulticast(localIP, IPAddress(224, 0, 0, 251), 5353);
#else
    mdnsSocket.beginMulticast(IPAddress(224, 0, 0, 251), 5353);
#endif
  }

  void end() {
    ArduinoOTAClass<NetServer, NetClient>::end();
    mdnsSocket.stop();
  }

  void poll() {
    ArduinoOTAClass<NetServer, NetClient>::poll();
    WiFiOTAClass::pollMdns(mdnsSocket);
  }

  void handle() { // alias
    poll();
  }

};

#ifndef NO_OTA_NETWORK

#if defined(ethernet_h_) || defined(ethernet_h) || defined(UIPETHERNET_H)
#define OTETHERNET
#endif
#if defined(UIPETHERNET_H) || defined(WIFIESPAT1) // no UDP multicast implementation
#define NO_OTA_PORT
#endif

#ifdef OTETHERNET
#ifdef NO_OTA_PORT
ArduinoOTAClass  <EthernetServer, EthernetClient>   ArduinoOTA;
#else
ArduinoOTAMdnsClass  <EthernetServer, EthernetClient, EthernetUDP>   ArduinoOTA;
#endif

#else

#ifdef NO_OTA_PORT
ArduinoOTAClass  <WiFiServer, WiFiClient> ArduinoOTA;
#else
#include <WiFiUdp.h>
ArduinoOTAMdnsClass <WiFiServer, WiFiClient, WiFiUDP> ArduinoOTA;
#endif
#endif

#endif

#endif
