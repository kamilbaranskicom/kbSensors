#pragma once

enum SensorType : uint8_t {
  SENSOR_UNKNOWN = 0,
  SENSOR_TEMPERATURE = 1,
  SENSOR_HUMIDITY = 2,
  SENSOR_ABSOLUTE_HUMIDITY = 3,
  SENSOR_PRESSURE = 4,
  SENSOR_AIR_QUALITY = 5,
  SENSOR_CO2_PPM = 6,
  SENSOR_VOC_PPB = 7
};

const char *sensorTypeStrs[] = {
    "unknown",                   // SENSOR_UNKNOWN
    "temperature",               // SENSOR_TEMP
    "humidity",                  // SENSOR_HUMIDITY
    "absolute_humidity",         // SENSOR_ABSOLUTE_HUMIDITY
    "pressure",                  // SENSOR_PRESSURE
    "air_quality",               // SENSOR_AIR_QUALITY
    "carbon_dioxide",            // SENSOR_CO2_PPM
    "volatile_organic_compounds_parts" // SENSOR_VOC_PPB
};

const char *sensorUnits[] = {
    "",      // SENSOR_UNKNOWN
    " °C",   // SENSOR_TEMP
    "%",     // SENSOR_HUMIDITY
    " g/m³", // SENSORS_ABSOLUTE_HUMIDITY
    " hPa",   // SENSOR_PRESSURE
    " µg/m³", // SENSOR_AIR_QUALITY
    " ppm",   // SENSOR_CO2_PPM
    " ppb"    // SENSOR_VOC_PPB

};

/*
   tables: sensorsDB
*/
// this is the sensors database, part of the array is stored on the device in
// LittleFS. (Addresses, compensations, friendly names).
struct SensorConfig {
  char address[17];         // 16 hex + \0
  char name[32];            // friendly name
  SensorType type;          // typ sensora
  float compensation;       // temperature reading correction
  bool present;             // runtime only
  float lastValue;          // last reading
  float lastPublishedValue; // runtime only; last mqtt published value
  uint32_t lastUpdate;      // timestamp of last reading
  // char valueType[6];     // "%" or " °C" or " g/m3" + \0
};

uint16_t sensorsCount = 0;
#define MAX_SENSORS 100
SensorConfig sensorsSettings[MAX_SENSORS];

/*************************************
   HW
*/

/*
   DHT11
*/
#include "DHTStable.h"
#define DHT11_PIN                                                                                                                          \
  4 // connect DHT11 to the D2 pin on NodeMCU v3 (don't forget about the \
     // resistor)
DHTStable DHT;

/*
   DS18B20
*/
#include <DallasTemperature.h>
#include <OneWire.h>

#define DS18B20_PIN 5 // Connect DS18B20 to D1 pin on NodeMCU v3 (don't forget about the resistor)
OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);
DeviceAddress Thermometer;

// forward definitions:
String getSensorType(const SensorConfig &s);
String getSensorUnits(const SensorConfig &s);