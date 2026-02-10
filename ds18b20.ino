#include "ds18b20.h"

bool ds18b20_init() { return true; }

bool ds18b20_update() {
  int deviceCount = 0;

  sensors.begin();
  sensors.requestTemperatures();

  uint8_t count = sensors.getDeviceCount();
  if (count == 0)
    return false;

  DeviceAddress addr;
  char addrStr[17];
  char defaultName[32];

  // Iterujemy po FIZYCZNIE znalezionych urządzeniach
  for (uint8_t i = 0; i < count; i++) {
    if (!sensors.getAddress(addr, i)) {
      Serial.println("Error getting address for sensor ");
      Serial.println(i);
      continue;
    }

    deviceAddressToString(addr, addrStr);

    float temp = sensors.getTempC(addr);

    if (temp == DEVICE_DISCONNECTED_C) {
      int16_t idx = findSensorByAddress(addrStr);
      String existingName = (idx >= 0) ? sensorsSettings[idx].name : "[not in config]";

      Serial.print("-- Skipping disconnected sensor: ");
      Serial.print(addrStr);
      Serial.print(" [");
      Serial.print(existingName);
      Serial.println("]");
      continue;
    }

    // Generowanie domyślnej nazwy (tylko na wypadek nowego sensora)
    snprintf(defaultName, sizeof(defaultName), "DS18B20-%02X%02X", addr[6], addr[7]);

    // 4. Upsert - dodaj lub zaktualizuj. Changed => jeżeli się coś zmieniło, informujemy wyżej.
    submitSensorReading(addrStr, temp, SENSOR_TEMPERATURE, defaultName);
    deviceCount++;
  }
  Serial.println("   =" + String(deviceCount) + " DS18B20 sensor(s) requested.");
  return true;
}

void deviceAddressToString(const uint8_t *addr, char *out) {
  const char hex[] = "0123456789ABCDEF";

  for (uint8_t i = 0; i < 8; i++) {
    out[i * 2] = hex[(addr[i] >> 4) & 0x0F];
    out[i * 2 + 1] = hex[addr[i] & 0x0F];
  }
  out[16] = '\0';
}

// Zamienia 16-znakowy string HEX na tablicę 8 bajtów
bool hexStringToAddress(const char *hexString, uint8_t *addr) {
  if (strlen(hexString) != 16)
    return false; // walidacja długości

  for (uint8_t i = 0; i < 8; i++) {
    char high = hexString[i * 2];
    char low = hexString[i * 2 + 1];

    auto hexToNibble = [](char c) -> int {
      if (c >= '0' && c <= '9')
        return c - '0';
      if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
      if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
      return -1; // invalid char
    };

    int h = hexToNibble(high);
    int l = hexToNibble(low);

    if (h == -1 || l == -1)
      return false; // niepoprawny znak HEX

    addr[i] = (h << 4) | l;
  }
  return true;
}