#include "dht11.h"

bool dht_scan() {
  if (DHT.read11(DHT11_PIN) != DHTLIB_OK) {
    Serial.println("DHT11 not found.");
    return false;
  }

  bool changed = false;
  changed |= registerSensor("DHTtemp", "DHT Temperature", SENSOR_TEMPERATURE);
  changed |= registerSensor("DHThumi", "DHT Humidity", SENSOR_HUMIDITY);
  changed |= registerSensor("DHTabsHumi", "DHT Abs Humidity", SENSOR_ABSOLUTE_HUMIDITY);
  return changed;
}

void dht_update() {
  if (DHT.read11(DHT11_PIN) != DHTLIB_OK) {
    Serial.println("DHT read error!");
    return;
  }

  float t = DHT.getTemperature();
  float h = DHT.getHumidity();
  float ah = (!isnan(t) && !isnan(h)) ? calculateAbsoluteHumidity(t, h) : NAN;

  for (uint16_t i = 0; i < sensorsCount; i++) {
    if (strcmp(sensorsSettings[i].address, "DHTtemp") == 0 && !isnan(t)) {
      sensorsSettings[i].lastValue = t;
      sensorsSettings[i].lastUpdate = millis();
    } else if (strcmp(sensorsSettings[i].address, "DHThumi") == 0 && !isnan(h)) {
      sensorsSettings[i].lastValue = h;
      sensorsSettings[i].lastUpdate = millis();
    } else if (strcmp(sensorsSettings[i].address, "DHTabsHumi") == 0 && !isnan(ah)) {
      sensorsSettings[i].lastValue = ah;
      sensorsSettings[i].lastUpdate = millis();
    }
  }
}

float calculateAbsoluteHumidity(float temperatureC, float relativeHumidity) {
  return (6.112 * exp((17.67 * temperatureC) / (temperatureC + 243.5)) * relativeHumidity * 2.1674) / (273.15 + temperatureC);
};