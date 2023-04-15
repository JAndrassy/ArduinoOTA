/*
 This example shows use of custom OTA storage with the ArduinoOTA library.
 The custom storage here is for Arduino Nano RP2040 Connect
 with the Arduino MBED based core.
 The custom storage is implemented in FileSystemStorage.h (part of this example).
 It stores the uploaded binary in file system in flash where
 the second stage bootloader provided by the SFU library expects it.
 */

#include <WiFiNINA.h>
#include <ArduinoOTA.h>
#include <SFU.h>
#include "FileSystemStorage.h"

#include "arduino_secrets.h" 
///////please enter your sensitive data in the Secret tab/arduino_secrets.h
/////// Wifi Settings ///////
char ssid[] = SECRET_SSID;      // your network SSID (name)
char pass[] = SECRET_PASS;   // your network password

int status = WL_IDLE_STATUS;

FileSystemStorageClass FSStorage;

void setup() {
  //Initialize serial:
  Serial.begin(9600);

  // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present");
    // don't continue:
    while (true);
  }

  // attempt to connect to Wifi network:
  while ( status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);
  }

  // start the WiFi OTA library with SD based storage
  ArduinoOTA.begin(WiFi.localIP(), "Arduino", "password", FSStorage);

  // you're connected now, so print out the status:
  printWifiStatus();

}

void loop() {
  // check for WiFi OTA updates
  ArduinoOTA.poll();

  // add your normal loop code below ...
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}
