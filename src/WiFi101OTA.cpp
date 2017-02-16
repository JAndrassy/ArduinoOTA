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

#include "WiFi101OTA.h"

#if defined(ARDUINO_SAMD_ZERO)
  #define BOARD "arduino_zero_edbg"
#elif defined(ARDUINO_SAMD_MKR1000)
  #define BOARD "mkr1000"
#elif defined(ARDUINO_SAMD_MKRZERO)
  #define BOARD "mkrzero"
#else
  #error "Unsupported board!"
#endif

#define BOARD_LENGTH (sizeof(BOARD) - 1)

WiFiOTAClass::WiFiOTAClass() :
  _storage(NULL),
  _server(65280),
  _lastMdnsResponseTime(0)
{
}

void WiFiOTAClass::begin(OTAStorage& storage)
{
  _storage = &storage;

  _server.begin();

  _mdnsSocket.beginMulti(IPAddress(224, 0, 0, 251), 5353);
}

void WiFiOTAClass::poll()
{
  pollMdns();
  pollServer();
}

void WiFiOTAClass::pollMdns()
{
  int packetLength = _mdnsSocket.parsePacket();

  if (packetLength <= 0) {
    return;
  }

  const byte ARDUINO_SERVICE_REQUEST[37] = {
    0x00, 0x00, // transaction id
    0x00, 0x00, // flags
    0x00, 0x01, // questions
    0x00, 0x00, // answer RRs
    0x00, 0x00, // authority RRs
    0x00, 0x00, // additional RRs
    0x08,
    0x5f, 0x61, 0x72, 0x64, 0x75, 0x69, 0x6e, 0x6f, // _arduino
    0x04, 
    0x5f, 0x74, 0x63, 0x70, // _tcp
    0x05,
    0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x00, // local
    0x00, 0x0c, // PTR
    0x00, 0x01 // Class IN
  };

  if (packetLength != sizeof(ARDUINO_SERVICE_REQUEST)) {
    while (_mdnsSocket.available()) {
      _mdnsSocket.read();
    }
    return;
  }

  byte request[packetLength];

  _mdnsSocket.read(request, sizeof(request));

  if (memcmp(request, ARDUINO_SERVICE_REQUEST, packetLength) != 0) {
    return;
  }

  if ((millis() - _lastMdnsResponseTime) < 1000) {
    // ignore request
    return;
  }
  _lastMdnsResponseTime = millis();

  _mdnsSocket.beginPacket(IPAddress(224, 0, 0, 251), 5353);

  const byte responseHeader[] = {
    0x00, 0x00, // transaction id
    0x84, 0x00, // flags
    0x00, 0x00, // questions
    0x00, 0x04, // answers RRs
    0x00, 0x00, // authority RRs
    0x00, 0x00  // additional RRS
  };
  _mdnsSocket.write(responseHeader, sizeof(responseHeader));

  const byte ptrRecord[] = {
    0x08,
    '_', 'a', 'r', 'd', 'u', 'i', 'n', 'o',
    
    0x04,
    '_', 't', 'c', 'p',

    0x05,
    'l', 'o', 'c', 'a', 'l',
    0x00,

    0x00, 0x0c, // PTR
    0x00, 0x01, // class IN
    0x00, 0x00, 0x11, 0x94, // TTL

    0x00, 0x0a, // length
    0x07,
     'A', 'r', 'd', 'u', 'i', 'n', 'o',
    0xc0, 0x0c
  };
  _mdnsSocket.write(ptrRecord, sizeof(ptrRecord));


  const byte txtRecord[] = {
    0xc0, 0x2b,
    0x00, 0x10, // TXT strings
    0x80, 0x01, // class
    0x00, 0x00, 0x11, 0x94, // TTL
    0x00, (34 + BOARD_LENGTH),
    13,
    's', 's', 'h', '_', 'u', 'p', 'l', 'o', 'a', 'd', '=', 'n', 'o',
    12,
    't', 'c', 'p', '_', 'c', 'h', 'e', 'c', 'k', '=', 'n', 'o',
    (6 + BOARD_LENGTH),
    'b', 'o', 'a', 'r', 'd', '=',
  };
  _mdnsSocket.write(txtRecord, sizeof(txtRecord));
  _mdnsSocket.write((byte*)BOARD, BOARD_LENGTH);

  const byte srvRecord[] = {
    0xc0, 0x2b, 
    0x00, 0x21, // SRV
    0x80, 0x01, // class
    0x00, 0x00, 0x00, 0x78, // TTL
    0x00, 0x10, // length
    0x00, 0x00,
    0x00, 0x00,
    0xff, 0x00, // port
    0x07,
    'A', 'r', 'd', 'u', 'i', 'n', 'o',
    0xc0, 0x1a
  };
  _mdnsSocket.write(srvRecord, sizeof(srvRecord));


  uint32_t localIp = WiFi.localIP();

  byte aRecordOffset = sizeof(ptrRecord) + sizeof(txtRecord) + BOARD_LENGTH + sizeof(srvRecord) + 2;

  byte aRecord[] = {
    0xc0, aRecordOffset,

    0x00, 0x01, // A record
    0x80, 0x01, // class
    0x00, 0x00, 0x00, 0x78, // TTL
    0x00, 0x04,
    0xff, 0xff, 0xff, 0xff // IP
  };
  memcpy(&aRecord[sizeof(aRecord) - 4], &localIp, sizeof(localIp));
  _mdnsSocket.write(aRecord, sizeof(aRecord));

  _mdnsSocket.endPacket();
}

void WiFiOTAClass::pollServer()
{
  WiFiClient client = _server.available();

  if (client) {
    String request = client.readStringUntil('\n');
    request.trim();

    if (request != "POST /sketch HTTP/1.1") {
      sendHttpResponse(client, 404, "Not Found");
      return;
    }

    String header;
    long contentLength = -1;

    do {
      header = client.readStringUntil('\n');
      header.trim();

      if (header.startsWith("Content-Length: ")) {
        header.remove(0, 16);

        contentLength = header.toInt();
      }
    } while (header != "");

    if (contentLength <= 0) {
      sendHttpResponse(client, 400, "Bad Request");
      return;
    }

    if (_storage == NULL || !_storage->open()) {
      sendHttpResponse(client, 500, "Internal Server Error");
      return;
    }

    long read = 0;

    while (client.connected() && read < contentLength) {
      if (client.available()) {
        read++;

        _storage->write((char)client.read());
      }
    }

    _storage->close();

    if (read == contentLength) {
      sendHttpResponse(client, 200, "OK");

      delay(250);

      // Reset the device
      NVIC_SystemReset() ;
      while (true);
    } else {
      _storage->clear();

      client.stop();
    }
  }
}

void WiFiOTAClass::sendHttpResponse(Client& client, int code, const char* status)
{
  while (client.available()) {
    client.read();
  }
  
  client.print("HTTP/1.1 ");
  client.print(code);
  client.print(" ");
  client.println(status);
  client.println("Connection: close");
  client.println();
  client.stop();
}

WiFiOTAClass WiFiOTA;
