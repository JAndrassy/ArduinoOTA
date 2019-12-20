/*

 This example polls for sketch updates over WiFi module on Serial1,
 sketches can be updated by selecting a network port from within
 the Arduino IDE: Tools -> Port -> Network Ports ...

 created 13 July 2010
 by dlf (Metodo2 srl)
 modified 31 May 2012
 by Tom Igoe
 modified 16 January 2017
 by Sandeep Mistry
 Serial WiFi version December 2019
 by Juraj Andrassy
 */

#include <WiFiEspAT.h>
#include <ArduinoOTA.h>

void setup() {

  Serial.begin(115200);
  while (!Serial);

  // initialize Serial1 for communication with the WiFi module
  Serial1.begin(115200);
  WiFi.init(&Serial1);

  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println();
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }

  // waiting for connection to Wifi network with settings in the WiFi module
  Serial.println("Waiting for connection to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print('.');
  }
  Serial.println();
  Serial.println("Connected to WiFi network.");

  // start the WiFi OTA library with internal (flash) based storage
  ArduinoOTA.begin(WiFi.localIP(), "Arduino", "password", InternalStorage);

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
