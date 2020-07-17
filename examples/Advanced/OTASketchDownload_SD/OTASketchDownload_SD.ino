/*

 This example downloads sketch update over network to SD card.
 It doesn't use the ArduionOTA library at all. It is intended for use
 with the SDU library of SAMD boards package or with SD bootloader for AVR.

 To create the bin file for update of a SAMD board (except of M0),
 use in Arduino IDE command "Export compiled binary".
 To create a bin file for AVR boards see the instructions in README.MD.
 To try this example, you should have a web server where you put
 the binary update.
 Modify the constants below to match your configuration.

 Created for ArduinoOTA library in February 2020
 by Juraj Andrassy
 */

#include <Ethernet.h>
#include <ArduinoHttpClient.h>
#include <SD.h>
#ifdef ARDUINO_ARCH_SAMD
#include <SDU.h> // prepends to this sketch a 'second stage SD bootloader'
  const char* BIN_FILENAME = "UPDATE.BIN"; // expected by the SDU library
#endif
#ifdef ARDUINO_ARCH_AVR
#include <avr/wdt.h> // for self reset
  const char* BIN_FILENAME = "FIRMWARE.BIN"; // expected by zevero avr_boot
#endif

const short VERSION = 1;

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

#ifndef SDCARD_SS_PIN
#define SDCARD_SS_PIN 4
#endif

#ifdef ARDUINO_SAM_ZERO // M0
#define Serial SerialUSB
#endif

void handleSketchDownload() {

  const char* SERVER = "192.168.1.108"; // must be string for HttpClient
  const unsigned short SERVER_PORT = 80;
  const char* PATH = "/update-v%d.bin";
  const unsigned long CHECK_INTERVAL = 5000;

  static unsigned long previousMillis;

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis < CHECK_INTERVAL)
    return;
  previousMillis = currentMillis;

  EthernetClient transport;
  HttpClient client(transport, SERVER, SERVER_PORT);

  char buff[32];
  snprintf(buff, sizeof(buff), PATH, VERSION + 1);

  Serial.print("Check for update file ");
  Serial.println(buff);

  client.get(buff);

  int statusCode = client.responseStatusCode();
  Serial.print("Update status code: ");
  Serial.println(statusCode);
  if (statusCode != 200) {
    client.stop();
    return;
  }

  long length = client.contentLength();
  if (length == HttpClient::kNoContentLengthHeader) {
    client.stop();
    Serial.println("Server didn't provide Content-length header. Can't continue with update.");
    return;
  }
  Serial.print("Server returned update file of size ");
  Serial.print(length);
  Serial.println(" bytes");

  File file = SD.open(BIN_FILENAME, O_CREAT | O_WRITE);
  if (!file) {
    client.stop();
    Serial.println("Could not create bin file. Can't continue with update.");
    return;
  }
  byte b;
  while (length > 0) {
    if (!client.readBytes(&b, 1)) // reading a byte with timeout
      break;
    file.write(b);
    length--;
  }
  file.close();
  client.stop();
  if (length > 0) {
    SD.remove(BIN_FILENAME);
    Serial.print("Timeout downloading update file at ");
    Serial.print(length);
    Serial.println(" bytes. Can't continue with update.");
    return;
  }
  Serial.println("Update file saved. Reset.");
  Serial.flush();
#ifdef __AVR__
  wdt_enable(WDTO_15MS);
  while (true);
#else
  NVIC_SystemReset();
#endif
}

void setup() {

  Serial.begin(115200);
  while (!Serial);

  Serial.print("Sketch version ");
  Serial.println(VERSION);

  pinMode(SDCARD_SS_PIN, OUTPUT);
  digitalWrite(SDCARD_SS_PIN, HIGH); // to disable SD card while Ethernet initializes

  Serial.println("Initialize Ethernet with DHCP:");
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    while (true);
  }
  Serial.println("Ethernet connected");

  Serial.print("Initializing SD card...");
  if (!SD.begin(SDCARD_SS_PIN)) {
    Serial.println("initialization failed!");
    // don't continue:
    while (true);
  }
  Serial.println("initialization done.");
  SD.remove(BIN_FILENAME);
}

void loop() {
  // check for updates
  handleSketchDownload();

  // add your normal loop code below ...
}
