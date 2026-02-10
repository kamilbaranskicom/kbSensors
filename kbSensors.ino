
const char programName[] = "kbSensors station.";
const char deviceName[] = "kbSensors";

const char programVersion[] = "0.20260210_k06";

const char programManual[] = "// tiny monitor station for one DHT11 and many DS18B20 sensors\r\n"
                             "// outputs data at http://kbsensors/ , http://kbsensors/xml , "
                             "http://kbsensors/txt\r\n"
                             "// https is not available for now. Optional ?refresh=[seconds] for html "
                             "output.";

/******************************************************************************
 *                                                                            *
 *  GLOBALS                                                                   *
 *                                                                            *
 ******************************************************************************/

/* uncomment the following line to use REMOTE_HELPER_SERVER instead of local
  files for: kbSensors.js, kbSensors.css and kbSensors.svg
*/
// #define REMOTE_HELPER_SERVER "http://192.168.50.3/kbSensors/"

// comment the following line to disable OTA updates
#define OTA_ENABLED

/*
   updates over the air
*/
#ifdef OTA_ENABLED
#include <ArduinoOTA.h>
#endif

/*
   friendly "kbsensors" name
*/
#define NETBIOSNAME "kbsensors"
#include <ESP8266SSDP.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>

#include "sensors_types.h"

#define WIFI_CONFIG_FILE "/wifi.json"

// Zmienne globalne do konfiguracji WiFi i logowania
String wifiSSID = "";
String wifiPassword = "";
String webUser = "admin";
String webPass = "admin";

/******************************************************************************
 *                                                                            *
 *  CODE                                                                      *
 *                                                                            *
 ******************************************************************************/

void setup() {
  initSerial();
  initResetButton();
#ifndef REMOTE_HELPER_SERVER
  initFileSystem();
#endif

  initWiFi();
#ifdef OTA_ENABLED
  initOTA();
#endif
  initNetbiosName();
  initWebserver();
  loadConfig();
  initSensors();
  initLED();
  initMQTT();
  initSSDP();
}

void loop() {
  // put your main code here, to run repeatedly:
  // updateSensorsValues();
  handleWebserver();
  checkResetButton();
  handleSensors();

#ifdef OTA_ENABLED
  ArduinoOTA.handle();
#endif

  mqttLoop();
  handleSerial();
}
