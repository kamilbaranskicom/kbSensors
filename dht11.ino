#include "dht11.h"

bool dht11_init() {
  if (DHT.read11(DHT11_PIN) != DHTLIB_OK) {
    Serial.println("DHT11 not found.");
    return false;
  }
  return true;
}

bool dht11_update() {
  if (DHT.read11(DHT11_PIN) != DHTLIB_OK) {
    return false;
  }

  float t = DHT.getTemperature();
  float h = DHT.getHumidity();

  // DHT Temperature
  submitSensorReading("DHTtemp", t, SENSOR_TEMPERATURE, "DHT Temperature");

  // DHT Humidity
  submitSensorReading("DHThumi", h, SENSOR_HUMIDITY, "DHT Humidity");

  // DHT Absolute Humidity
  if (!isnan(t) && !isnan(h)) {
    float ah = calculateAbsoluteHumidity(t, h);
    submitSensorReading("DHTabsHumi", ah, SENSOR_ABSOLUTE_HUMIDITY, "DHT Abs Humidity");
  }
  return true;
}

float calculateAbsoluteHumidity(float temperatureC, float relativeHumidity) {
  return (6.112 * exp((17.67 * temperatureC) / (temperatureC + 243.5)) * relativeHumidity * 2.1674) / (273.15 + temperatureC);
};