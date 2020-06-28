/*
MQTT DHCP OTA Ethernet example
Created on 28-june-2020

This example allows for updating using MQTT command

Required
MQTT Server
HTTP Webserver that your device can access
Hardware with optiboot bootloader

Libs
https://github.com/arduino-libraries/ArduinoHttpClient
https://github.com/jandrassy/ArduinoOTA
https://github.com/knolleary/pubsubclient

Fill the below settings
Upload your bin to the webserver
Post the path (including /) to topic firmware
This should trigger a download and flash bin file
*/

#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoOTA.h>
#include <HttpClient.h>
#include <PubSubClient.h>
#ifndef VERSION
  #define VERSION 1
#endif

// Settings: Update these with values suitable for your hardware/network.
const char* UPDATE_SERVER = "some.http.server"; // must be string for HttpClient
const unsigned short UPDATE_SERVER_PORT = 80;

const char* MQTT_SERVER = "some.mqtt.server";
const char* MQTT_CLIENT = "ArduinoMQTT";
const char* MQTT_USER = "mqttuser";
const char* MQTT_PASS = "mqttpassword";

byte mac[]    = {  0xDE, 0xED, 0xBA, 0xFE, 0xFE, 0xED };

// End settings

EthernetClient ethClient;
PubSubClient mqttclient(ethClient);
HttpClient httpclient(ethClient, UPDATE_SERVER, UPDATE_SERVER_PORT);

long lastReconnectAttempt = 0;

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (unsigned int i=0;i<length;i++) {
    Serial.write((char)payload[i]);
  }
  Serial.println();
  handleSketchDownload(payload, length);
}

void handleSketchDownload(byte* payload, unsigned int payloadlength) {
  char buf[payloadlength+2];
  buf[payloadlength+1] = 0x00;
  
  uint8_t copyIndex = 0;
  if(payload[0] != '/') {
    buf[copyIndex] = '/';
    copyIndex++;
  }
  snprintf(&buf[copyIndex], payloadlength+1, payload);
  mqttclient.disconnect();

  Serial.print("Check for update file ");
  Serial.println(buf);

  httpclient.get(buf);

  int statusCode = httpclient.responseStatusCode();
  Serial.print("Update status code: ");
  Serial.println(statusCode);
  if (statusCode != 200) {
    httpclient.stop();
    return;
  }

  long length = httpclient.contentLength();
  if (length == HttpClient::kNoContentLengthHeader) {
    httpclient.stop();
    Serial.println("Server didn't provide Content-length header. Can't continue with update.");
    return;
  }
  Serial.print("Server returned update file of size ");
  Serial.print(length);
  Serial.println(" bytes");

  if (!InternalStorage.open(length)) {
    httpclient.stop();
    Serial.println("There is not enough space to store the update. Can't continue with update.");
    return;
  }
  byte b;
  while (length > 0) {
    if (!httpclient.readBytes(&b, 1)) // reading a byte with timeout
      break;
    InternalStorage.write(b);
    length--;
  }
  InternalStorage.close();
  httpclient.stop();
  if (length > 0) {
    Serial.print("Timeout downloading update file at ");
    Serial.print(length);
    Serial.println(" bytes. Can't continue with update.");
    return;
  }

  Serial.println("Sketch update apply and reset.");
  Serial.flush();
  InternalStorage.apply(); // this doesn't return
}


boolean reconnect() {
  Serial.println("(re)connecting");
  if (mqttclient.connect(MQTT_CLIENT, MQTT_USER, MQTT_PASS)) {
    // Once connected, publish an announcement...
    mqttclient.publish("outTopic","hello world");
    // ... and resubscribe
    mqttclient.subscribe("firmware");
  }
  return mqttclient.connected();
}

void setup()
{
  Serial.begin(9600);
  Serial.println(F("Starting MQTT DHCP OTA example COM0"));
  Serial.print("Sketch version ");
  Serial.println(VERSION);
  
  // start the Ethernet connection:
  Serial.println("Initialize Ethernet with DHCP:");
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
    } else if (Ethernet.linkStatus() == LinkOFF) {
      Serial.println("Ethernet cable is not connected.");
    }
    // no point in carrying on, so do nothing forevermore:
    while (true) {
      delay(1);
    }
  }
  // print your local IP address:
  Serial.print("My IP address: ");
  Serial.println(Ethernet.localIP());
  
  mqttclient.setServer(MQTT_SERVER, 1883);
  mqttclient.setCallback(callback);

  delay(1500);
  lastReconnectAttempt = 0;
}


void loop()
{
    switch (Ethernet.maintain()) {
    case 1:
      //renewed fail
      Serial.println("Error: renewed fail");
      break;

    case 2:
      //renewed success
      Serial.println("Renewed success");
      //print your local IP address:
      Serial.print("My IP address: ");
      Serial.println(Ethernet.localIP());
      break;

    case 3:
      //rebind fail
      Serial.println("Error: rebind fail");
      break;

    case 4:
      //rebind success
      Serial.println("Rebind success");
      //print your local IP address:
      Serial.print("My IP address: ");
      Serial.println(Ethernet.localIP());
      break;

    default:
      //nothing happened
      break;
  }
  if (!mqttclient.connected()) {
    long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    // Client connected
    mqttclient.loop();
  }
}
