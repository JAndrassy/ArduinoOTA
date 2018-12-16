/*

 This example polls for sketch updates over Ethernet, sketches
 can be updated by selecting a network port from within
 the Arduino IDE: Tools -> Port -> Network Ports ...

 Circuit:
 * W5100, W5200 or W5500 Ethernet shield with SD card attached

 created 13 July 2010
 by dlf (Metodo2 srl)
 modified 31 May 2012
 by Tom Igoe
 modified 16 January 2017
 by Sandeep Mistry
 Ethernet version August 2018
 by Juraj Andrassy
 */
 
#include <SPI.h>
#include <SD.h>
#include <Ethernet.h>
#include <ArduinoOTA.h>
#include <SDU.h>

//#define Serial SerialUSB

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

void setup() {
  //Initialize serial:
  Serial.begin(9600);
  while (!Serial);

  // setup SD card
  Serial.print("Initializing SD card...");
  if (!SD.begin(SDCARD_SS_PIN)) {
    Serial.println("initialization failed!");
    // don't continue:
    while (true);
  }
  Serial.println("initialization done.");

  // start the Ethernet connection:
  Serial.println("Initialize Ethernet with DHCP:");
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
  } else {
    Serial.print("  DHCP assigned IP ");
    Serial.println(Ethernet.localIP());
  }

  // start the OTEthernet library with SD based storage
  ArduinoOTA.begin(Ethernet.localIP(), "Arduino", "password", SDStorage);

}

void loop() {
  // check for updates
  ArduinoOTA.poll();

  // add your normal loop code below ...
}
