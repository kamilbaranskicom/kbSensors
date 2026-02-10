#include "ds18b20.h"

bool ds18b20_scan() {
  bool changed = false;
  DeviceAddress addr;
  sensors.begin();
  delay(50); // mały czas dla OneWire

  uint8_t found = sensors.getDeviceCount();
  Serial.printf("DS18B20 scan: %u sensors found\n", found);

  for (uint8_t i = 0; i < found; i++) {
    if (!sensors.getAddress(addr, i)) {
      Serial.printf("Failed to get address for sensor %u\n", i);
      continue;
    }

    char addrStr[17];
    for (uint8_t j = 0; j < 8; j++)
      sprintf(&addrStr[j * 2], "%02X", addr[j]);
    addrStr[16] = '\0';

    char defaultName[32];
    snprintf(defaultName, sizeof(defaultName), "DS18B20-%d", i + 1);

    changed |= registerSensor(addrStr, defaultName, SENSOR_TEMPERATURE);
  }
  return changed;
}

void ds18b20_update() {
  uint8_t deviceCount = 0;
  sensors.requestTemperatures();
  for (uint16_t i = 0; i < sensorsCount; i++) {
    // Interesują nas tylko te, które mają 16-znakowe adresy (HEX) i nie są DHT/CCS
    if (strlen(sensorsSettings[i].address) == 16 && sensorsSettings[i].type == SENSOR_TEMPERATURE) {
      DeviceAddress addr;
      if (hexStringToAddress(sensorsSettings[i].address, addr)) {
        float temp = sensors.getTempC(addr);
        if (temp != DEVICE_DISCONNECTED_C) {
          sensorsSettings[i].lastValue = temp;
          sensorsSettings[i].lastUpdate = millis();
          sensorsSettings[i].present = true;
          Serial.println("    sensor_" + (String)sensorsSettings[i].address + " [" + (String)sensorsSettings[i].name +
                         "] = " + (String)sensorsSettings[i].lastValue + sensorUnits[SENSOR_TEMPERATURE] + ".");
          deviceCount++;
        } else {
          Serial.println("-- Skipping disconnected (?) sensor_" + (String)sensorsSettings[i].address + " [" +
                         (String)sensorsSettings[i].name + "] = " + (String)temp);
        }
      }
    }
  }
  Serial.println("   =" + String(deviceCount) + " DS18B20 sensor(s) requested.");
}
