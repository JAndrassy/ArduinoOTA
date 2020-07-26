/*

 This example downloads sketch update over network.
 It doesn't start the OTA upload sever of the ArduinoOTA library,
 it only uses the InternalStorage object of the library
 to store and apply the downloaded binary file.

 To create the bin file for update of a SAMD board (except of M0),
 use in Arduino IDE command "Export compiled binary".
 To create a bin file for AVR boards see the instructions in README.MD.
 To try this example, you should have a web server where you put
 the binary update.
 Modify the constants below to match your configuration.

 Created for ArduinoOTA library in February 2020
 by Juraj Andrassy
 */

#include <ArduinoOTA.h> // only for InternalStorage
#include <Ethernet.h>
#include <ArduinoHttpClient.h>

const short VERSION = 1;

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

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

  if (!InternalStorage.open(length)) {
    client.stop();
    Serial.println("There is not enough space to store the update. Can't continue with update.");
    return;
  }
  byte b;
  while (length > 0) {
    if (!client.readBytes(&b, 1)) // reading a byte with timeout
      break;
    InternalStorage.write(b);
    length--;
  }
  InternalStorage.close();
  client.stop();
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

void setup() {

  Serial.begin(115200);
  while (!Serial);

  Serial.print("Sketch version ");
  Serial.println(VERSION);

  Serial.println("Initialize Ethernet with DHCP:");
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    while (true);
  }
  Serial.println("Ethernet connected");
}

void loop() {
  // check for updates
  handleSketchDownload();

  // add your normal loop code below ...
}
