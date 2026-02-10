#ifndef DHT11_H
#define DHT11_H

/*
   DHT11 sensor(s)
*/

#include "DHTStable.h"
#define DHT11_PIN                                                                                                                          \
  4 // connect DHT11 to the D2 pin on NodeMCU v3 (don't forget about the \
     // resistor)
DHTStable DHT;

#endif // DHT11_H