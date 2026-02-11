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

  // defaults
  float t = 25.0;
  float h = 40.0;

  // use environment data if it's fresh (hardcoded max 10 minutes)
  unsigned long now = millis();
  if (now - globalEnv.lastTempUpdate < 600000)
    t = globalEnv.temperature;
  if (now - globalEnv.lastHumiUpdate < 600000)
    h = globalEnv.humidity;

  // Aplikujemy do sensora
  ccs.setEnvironmentalData(h, t);

  float eco2 = ccs.geteCO2();
  float tvoc = ccs.getTVOC();

  submitSensorReading("CCS811eCO2", eco2, SENSOR_ECO2_PPM, "eCO2");
  submitSensorReading("CCS811TVOC", tvoc, SENSOR_TVOC_PPB, "TVOC");

  Serial.printf("CCS811 read (comp: %.1fC %.1f%%): eCO2=%.0f TVOC=%.0f\r\n", t, h, eco2, tvoc);
  return true;
}