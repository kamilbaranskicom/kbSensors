
const char programName[] = "KB Sensors station.";

const char programVersion[] = "0.20251225_k03";

const char programManual[] =
  "// tiny monitor station for one DHT11 and many DS18B20 sensors\r\n"
  "// outputs data at http://kbsensors/ , http://kbsensors/xml , http://kbsensors/txt\r\n"
  "// https is not available for now. Optional ?refresh=[seconds] for html output.";




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
   filesystem
*/
#include <LittleFS.h>
const char *fsName = "LittleFS";
FS *fileSystem = &LittleFS;
LittleFSConfig fileSystemConfig = LittleFSConfig();
static bool fsOK;


/*
   friendly "kbsensors" name
*/
#define NETBIOSNAME "kbsensors"


/*
   updates over the air
*/
#ifdef OTA_ENABLED
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>

#endif

/*************************************
   SENSORS:
*/

/*
   DHT11
*/
#include "DHTStable.h"
#define DHT11_PIN 4  // connect DHT11 to the D2 pin on NodeMCU v3 (don't forget about the resistor)
DHTStable DHT;

/*
   DS18B20
*/
#include <DallasTemperature.h>
#include <OneWire.h>

#define DS18B20_PIN 5  // Connect DS18B20 to D1 pin on NodeMCU v3 (don't forget about the resistor)
OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);
DeviceAddress Thermometer;

#define MAX_SENSORS 100

#include "sensors_types.h"

const char* sensorTypeStrs[] = {
    "unknown",       // SENSOR_UNKNOWN
    "temperature",   // SENSOR_TEMP
    "humidity",      // SENSOR_HUMIDITY
//    "pressure",      // SENSOR_PRESSURE
//    "air_quality"    // SENSOR_AIR_QUALITY
};

const char* sensorUnits[] = {
    "",      // SENSOR_UNKNOWN
    " °C",   // SENSOR_TEMP
    "%",      // SENSOR_HUMIDITY
//    "hPa",      // SENSOR_PRESSURE
//    "µg/m³"    // SENSOR_AIR_QUALITY
};

/*
   tables: sensorsDB
*/
// this is the sensors database, part of the array is stored on the device in LittleFS. (Addresses, compensations, friendly names).
struct SensorConfig {
  char address[17];     // 16 hex + \0
  char name[32];        // friendly name
  float compensation;   // temperature reading correction
  bool present;         // runtime only
  float lastValue;      // last reading
  float lastPublishedValue; // runtime only; last mqtt published value
  uint32_t lastUpdate;  // timestamp of last reading
  char valueType[5];    // "%" or " °C" + \0
  SensorType type;      // typ sensora
};

uint16_t sensorsCount = 0;
SensorConfig sensorsSettings[MAX_SENSORS];


#define WIFI_CONFIG_FILE "/wifi.json"

// Zmienne globalne do konfiguracji WiFi i logowania
String wifiSSID = "";
String wifiPassword = "";
String webUser = "admin";
String webPass = "admin";

extern String mqttDeviceId;
#define MQTT_MAX_PACKET_SIZE 768
#include <PubSubClient.h>



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
