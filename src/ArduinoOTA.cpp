#include "ArduinoOTA.h"

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