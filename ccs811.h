#ifndef CCS811_H
#define CCS811_H

/*
   CCS811 sensor
*/

#include "Adafruit_CCS811.h"
#include <Wire.h>

// Using safe pins on the right side: D5 (GPIO14) and D6 (GPIO12)
// Left side GPIOs are hardwired to internal Flash and cause bootloops
#define I2C_SDA 12
#define I2C_SCL 14

Adafruit_CCS811 ccs;

#endif // CCS811_H