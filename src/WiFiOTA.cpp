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
 modified for ArduinoOTA Dec 2018, Apr 2019
 by Juraj Andrassy
*/

#include <Arduino.h>

#include "WiFiOTA.h"

#define BOARD "arduino"
#define BOARD_LENGTH (sizeof(BOARD) - 1)

static String base64Encode(const String& in)
{
  static const char* CODES = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

  int b;
  String out;
  out.reserve((in.length()) * 4 / 3);
  
  for (unsigned int i = 0; i < in.length(); i += 3) {
    b = (in.charAt(i) & 0xFC) >> 2;
    out += CODES[b];

    b = (in.charAt(i) & 0x03) << 4;
    if (i + 1 < in.length()) {
      b |= (in.charAt(i + 1) & 0xF0) >> 4;
      out += CODES[b];
      b = (in.charAt(i + 1) & 0x0F) << 2;
      if (i + 2 < in.length()) {
         b |= (in.charAt(i + 2) & 0xC0) >> 6;
         out += CODES[b];
         b = in.charAt(i + 2) & 0x3F;
         out += CODES[b];
      } else {
        out += CODES[b];
        out += '=';
      }
    } else {
      out += CODES[b];
      out += "==";
    }
  }

  return out;
}

WiFiOTAClass::WiFiOTAClass() :
  _storage(NULL),
  localIp(0),
  _lastMdnsResponseTime(0),
  beforeApplyCallback(nullptr)
{
}

void WiFiOTAClass::begin(IPAddress& localIP, const char* name, const char* password, OTAStorage& storage)
{
  localIp = localIP;
  _name = name;
  _expectedAuthorization = "Basic " + base64Encode("arduino:" + String(password));
  _storage = &storage;
}

void WiFiOTAClass::pollMdns(UDP &_mdnsSocket)
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
    while (packetLength) {
      if (_mdnsSocket.available()) {
        packetLength--;
        _mdnsSocket.read();
      }
    }
    return;
  }

  byte request[packetLength];

  _mdnsSocket.read(request, sizeof(request));

  if (memcmp(&request[2], &ARDUINO_SERVICE_REQUEST[2], packetLength - 2) != 0) {
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

  const byte ptrRecordStart[] = {
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

    0x00, (byte)(_name.length() + 3), // length
    (byte)_name.length()
  };

  const byte ptrRecordEnd[] = {
    0xc0, 0x0c
  };

  _mdnsSocket.write(ptrRecordStart, sizeof(ptrRecordStart));
  _mdnsSocket.write((const byte*) _name.c_str(), _name.length());
  _mdnsSocket.write(ptrRecordEnd, sizeof(ptrRecordEnd));

  const byte txtRecord[] = {
    0xc0, 0x2b,
    0x00, 0x10, // TXT strings
    0x80, 0x01, // class
    0x00, 0x00, 0x11, 0x94, // TTL
    0x00, (50 + BOARD_LENGTH),
    13,
    's', 's', 'h', '_', 'u', 'p', 'l', 'o', 'a', 'd', '=', 'n', 'o',
    12,
    't', 'c', 'p', '_', 'c', 'h', 'e', 'c', 'k', '=', 'n', 'o',
    15,
    'a', 'u', 't', 'h', '_', 'u', 'p', 'l', 'o', 'a', 'd', '=', 'y', 'e', 's',
    (6 + BOARD_LENGTH),
    'b', 'o', 'a', 'r', 'd', '=',
  };
  _mdnsSocket.write(txtRecord, sizeof(txtRecord));
  _mdnsSocket.write((byte*)BOARD, BOARD_LENGTH);

  const byte srvRecordStart[] = {
    0xc0, 0x2b, 
    0x00, 0x21, // SRV
    0x80, 0x01, // class
    0x00, 0x00, 0x00, 0x78, // TTL
    0x00, (byte)(_name.length() + 9), // length
    0x00, 0x00,
    0x00, 0x00,
    0xff, 0x00, // port
    (byte)_name.length()
  };

  const byte srvRecordEnd[] = {
    0xc0, 0x1a
  };

  _mdnsSocket.write(srvRecordStart, sizeof(srvRecordStart));
  _mdnsSocket.write((const byte*) _name.c_str(), _name.length());
  _mdnsSocket.write(srvRecordEnd, sizeof(srvRecordEnd));

  byte aRecordNameOffset = sizeof(responseHeader) +
                            sizeof(ptrRecordStart) + _name.length() + sizeof(ptrRecordEnd) + 
                            sizeof(txtRecord) + BOARD_LENGTH +
                            sizeof(srvRecordStart) - 1;

  byte aRecord[] = {
    0xc0, aRecordNameOffset,

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

void WiFiOTAClass::pollServer(Client& client)
{

  if (client) {
    String request = client.readStringUntil('\n');
    request.trim();

    String header;
    long contentLength = -1;
    String authorization;

    do {
      header = client.readStringUntil('\n');
      header.trim();

      if (header.startsWith("Content-Length: ")) {
        header.remove(0, 16);

        contentLength = header.toInt();
      } else if (header.startsWith("Authorization: ")) {
        header.remove(0, 15);

        authorization = header;
      }
    } while (header != "");

    bool dataUpload = false;
#if defined(ESP8266) || defined(ESP32)
    if (request == "POST /data HTTP/1.1") {
      dataUpload = true;
    } else
#endif
    if (request != "POST /sketch HTTP/1.1") {
      flushRequestBody(client, contentLength);
      sendHttpResponse(client, 404, "Not Found");
      return;
    }

    if (_expectedAuthorization != authorization) {
      flushRequestBody(client, contentLength);
      sendHttpResponse(client, 401, "Unauthorized");
      return;
    }

    if (contentLength <= 0) {
      sendHttpResponse(client, 400, "Bad Request");
      return;
    }

    if (_storage == NULL || !_storage->open(contentLength, dataUpload)) {
      flushRequestBody(client, contentLength);
      sendHttpResponse(client, 500, "Internal Server Error");
      return;
    }

    if (contentLength > _storage->maxSize()) {
      _storage->close();
      flushRequestBody(client, contentLength);
      sendHttpResponse(client, 413, "Payload Too Large");
      return;
    }

    long read = 0;
    byte buff[64];

    while (client.connected() && read < contentLength) {
      while (client.available()) {
        int l = client.read(buff, sizeof(buff));
        for (int i = 0; i < l; i++) {
          _storage->write(buff[i]);
        }
        read += l;
      }
    }

    _storage->close();

    if (read == contentLength) {
      sendHttpResponse(client, 200, "OK");

      delay(500);

      if (beforeApplyCallback) {
        beforeApplyCallback();
      }

      // apply the update
      _storage->apply();
      
      while (true);
    } else {

      sendHttpResponse(client, 414, "Payload size wrong");
      _storage->clear();

      delay(500);

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
  delay(500);
  client.stop();
}

void WiFiOTAClass::flushRequestBody(Client& client, long contentLength)
{
  long read = 0;

  while (client.connected() && read < contentLength) {
    if (client.available()) {
      read++;

      client.read();
    }
  }
}

