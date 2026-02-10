#ifndef DS18B20_H
#define DS18B20_H

/*
   DS18B20 sensors
*/

#include <DallasTemperature.h>
#include <OneWire.h>

#define DS18B20_PIN 5 // Connect DS18B20 to D1 pin on NodeMCU v3 (don't forget about the resistor)
OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);
DeviceAddress Thermometer;

#endif // DS18B20_H