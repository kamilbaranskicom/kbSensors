#include "ccs811.h"

bool ccs811_scan() {
  if (!ccs.begin()) {
    Serial.println("CCS811 not found.");
    return false;
  }

  bool changed = false;
  changed |= registerSensor("CCS811eCO2", "eCO2", SENSOR_ECO2_PPM);
  changed |= registerSensor("CCS811TVOC", "TVOC", SENSOR_TVOC_PPB);
  return changed;
}

void ccs811_update() {
  if (ccs.available() && !ccs.readData()) {
    float co2 = ccs.geteCO2();
    float voc = ccs.getTVOC();

    for (uint16_t i = 0; i < sensorsCount; i++) {
      if (strcmp(sensorsSettings[i].address, "CCS811eCO2") == 0) {
        Serial.println("CCS811 eCO2: " + String(co2) + " ppm");
        sensorsSettings[i].lastValue = co2;
        sensorsSettings[i].lastUpdate = millis();
      } else if (strcmp(sensorsSettings[i].address, "CCS811TVOC") == 0) {
        Serial.println("CCS811 TVOC: " + String(voc) + " ppb");
        sensorsSettings[i].lastValue = voc;
        sensorsSettings[i].lastUpdate = millis();
      }
    }
  }
}