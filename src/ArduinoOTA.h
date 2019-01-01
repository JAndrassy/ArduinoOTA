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


template <class NetServer, class NetClient>
class ArduinoOTAClass : public WiFiOTAClass {

private:
  NetServer server;

public:
  ArduinoOTAClass() : server(65280) {};

  void begin(IPAddress localIP, const char* name, const char* password, OTAStorage& storage) {
    WiFiOTAClass::begin(localIP, name, password, storage);
    server.begin();
  }

  void poll() {
    NetClient client = server.available();
    pollServer(client);
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
    mdnsSocket.beginMulticast(IPAddress(224, 0, 0, 251), 5353);
  }

  void poll() {
    ArduinoOTAClass<NetServer, NetClient>::poll();
    WiFiOTAClass::pollMdns(mdnsSocket);
  }

};

#if defined(ethernet_h_) || defined(ethernet_h) // Ethernet library
ArduinoOTAMdnsClass  <EthernetServer, EthernetClient, EthernetUDP>   ArduinoOTA;

#elif defined(WiFiNINA_h) || defined(WIFI_H) // NINA and WiFi101
#include <WiFiUdp.h>
ArduinoOTAMdnsClass <WiFiServer, WiFiClient, WiFiUDP> ArduinoOTA;

#elif defined(WiFi_h) // WiFi and WiFiLink lib (no UDP multicast), for WiFiLink firmware handles mdns
ArduinoOTAClass  <WiFiServer, WiFiClient> ArduinoOTA;

#elif defined(_WIFISPI_H_INCLUDED) // no UDP multicast implementation
ArduinoOTAClass  <WiFiSpiServer, WiFiSpiClient> ArduinoOTA;

#elif defined(UIPETHERNET_H) // no UDP multicast implementation
ArduinoOTAClass  <EthernetServer, EthernetClient>   ArduinoOTA;

#else
#error "Network library not included or not supported"
#endif

#endif

