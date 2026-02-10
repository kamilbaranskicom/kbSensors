#include "ccs811.h"

static bool ccsInitialized = false;

bool ccs811_init() {
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!ccs.begin()) {
    Serial.println("CCS811 not found.");
    ccsInitialized = false;
    return false;
  }

  ccsInitialized = true;
  Serial.println("CCS811 found and initialized successfully.");
  return true;
}

bool ccs811_update() {
  if (!ccsInitialized) {
    if (!ccs811_init()) {
      return false;
    }
  }

  if (!ccs.available()) {
    Serial.println("CCS811 data not available (yet?).");
    return false;
  }

  if (ccs.readData()) { // false on success, true on error
    Serial.println("Error reading CCS811 data.");
    ccsInitialized = false; // Force reinitialization on next update
    return false;
  }

  submitSensorReading("CCS811eCO2", ccs.geteCO2(), SENSOR_ECO2_PPM, "eCO2");
  submitSensorReading("CCS811TVOC", ccs.getTVOC(), SENSOR_TVOC_PPB, "TVOC");
  return true;
}