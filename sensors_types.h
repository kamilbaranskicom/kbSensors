#pragma once

enum SensorType : uint8_t {
  SENSOR_UNKNOWN = 0,
  SENSOR_TEMPERATURE = 1,
  SENSOR_HUMIDITY = 2,
  SENSOR_ABSOLUTE_HUMIDITY = 3,
  SENSOR_PRESSURE = 4,
  SENSOR_AIR_QUALITY = 5,
  SENSOR_ECO2_PPM = 6,
  SENSOR_TVOC_PPB = 7
};

const char *sensorTypeStrs[] = {
    "unknown",                         // SENSOR_UNKNOWN
    "temperature",                     // SENSOR_TEMP
    "humidity",                        // SENSOR_HUMIDITY
    "absolute_humidity",               // SENSOR_ABSOLUTE_HUMIDITY
    "pressure",                        // SENSOR_PRESSURE
    "air_quality",                     // SENSOR_AIR_QUALITY
    "carbon_dioxide",                  // SENSOR_CO2_PPM
    "volatile_organic_compounds_parts" // SENSOR_VOC_PPB
};

const char *sensorUnits[] = {
    "",       // SENSOR_UNKNOWN
    " °C",    // SENSOR_TEMP
    "%",      // SENSOR_HUMIDITY
    " g/m³",  // SENSORS_ABSOLUTE_HUMIDITY
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

/*
 * Global environment imports
 */
enum ImportMethod : uint8_t {
  //
  IMPORT_NONE = 0,
  IMPORT_MQTT_JSON = 1, // Nasłuchuj tematu MQTT (oczekuje JSON {"value": 12.3})
  IMPORT_HTTP_JSON = 2, // Pobierz URL (oczekuje struktury kbSensors {"sensors": [...]})
  IMPORT_LOCAL = 3      // Z tego samego urządzenia (RAM)
};

// Konfiguracja pojedynczego kanału importu (np. dla Temperatury)
struct EnvCompensationConfig {
  ImportMethod method;
  String source;       // URL (http://192...) lub Temat MQTT (kbSensors/ID/...)
  char address[17];    // Adres sensora w JSON (np. "28D8BE..." lub "DHTtemp")
  float fallbackValue; // Wartość domyślna, gdy brak odczytu
};

// Struktura trzymająca globalne warunki środowiskowe dla kompensacji
struct GlobalEnvironment {
  float temperature;
  float humidity;
  unsigned long lastTempUpdate;
  unsigned long lastHumiUpdate;
};

GlobalEnvironment globalEnv = {
    //
    25.0f,
    50.0f,
    0,
    0};

// Dodajemy to do głównego Configu (deklaracja w sensors_types.h, instancja w kbSensors.ino)
struct AppConfig {
  // ... istniejące pola ...
  EnvCompensationConfig compTemp;
  EnvCompensationConfig compHumi;
};

AppConfig config;

// forward definitions:
String getSensorType(const SensorConfig &s);
String getSensorUnits(const SensorConfig &s);
