unsigned long lastScanTime = 0;
unsigned long currentWaitInterval = 60000;    // Current wait interval (can be changed by force update)
const unsigned long SCAN_INTERVAL_MS = 60000; // 60 seconds
// the sensors are slow; you can read them not more often than 1s.
const unsigned long FLOOD_GUARD_MS = 1000; // 1 second

void initSensors() {
  // sensors.begin();  // ds18b20 init
  scanSensors();
  registerForceUpdate(0);
}

void handleSensors() {
  unsigned long now = millis();
  unsigned long elapsed = now - lastScanTime;

  // 1. Flood Guard - absolutny bezpiecznik (arytmetyka unsigned long chroni przed overflow)
  if (elapsed < FLOOD_GUARD_MS) {
    return;
  }

  // 2. Sprawdzamy, czy upłynął zaplanowany czas (zwykły lub wymuszony)
  if (elapsed >= currentWaitInterval) {
    Serial.println("Executing sensor read...");

    // --- MIEJSCE NA TWOJE ODCZYTY (np. readDht, readVcc itp.) ---
    scanSensors();
    updateDHTValues();
    updateDS18B20Values();

    // 3. Po wykonaniu odczytu:
    lastScanTime = millis();                // Resetujemy czas bazowy
    currentWaitInterval = SCAN_INTERVAL_MS; // Przywracamy domyślną minutę

    Serial.println("Scan finished. Next regular scan in 60s.");
  }
  mqttPublishChangedSensors();
}

void registerForceUpdate(int intervalSeconds) {
  unsigned long now = millis();
  unsigned long elapsed = now - lastScanTime; // Ile czasu już minęło od ostatniego razu
  unsigned long requestedTotalInterval = elapsed + (intervalSeconds * 1000ULL);

  // Jeśli nowy całkowity czas oczekiwania jest krótszy niż ten, który mamy zaplanowany...
  if (requestedTotalInterval < currentWaitInterval) {
    currentWaitInterval = requestedTotalInterval;

    // Zabezpieczenie: interwał nie może być krótszy niż flood guard
    if (currentWaitInterval < FLOOD_GUARD_MS) {
      currentWaitInterval = FLOOD_GUARD_MS;
    }

    Serial.printf("New interval set: trigger in %ds", intervalSeconds);
    Serial.println();
  } else {
    Serial.println("Force update ignored - existing schedule is sooner.");
  }
}

void updateDHTValues() {
  float temperature = NAN;
  float humidity = NAN;
  float absoluteHumidity = NAN;

  int chk = DHT.read11(DHT11_PIN);
  if (chk != DHTLIB_OK) {
    Serial.println("DHT read error!");
    return;
  }

  temperature = DHT.getTemperature();
  humidity = DHT.getHumidity();
  if (!isnan(temperature) && !isnan(humidity)) {
    absoluteHumidity = calculateAbsoluteHumidity(temperature, humidity);
  }

  for (uint16_t i = 0; i < sensorsCount; i++) {
    if (strcmp(sensorsSettings[i].address, "DHTtemp") == 0) {
      if (!isnan(temperature)) {
        sensorsSettings[i].lastValue = temperature;
        sensorsSettings[i].lastUpdate = millis();
        sensorsSettings[i].present = true;
        sensorsSettings[i].type = SENSOR_TEMPERATURE;
        strcpy(sensorsSettings[i].valueType, sensorUnits[SENSOR_TEMPERATURE]);
        Serial.printf("    DHT temperature: %s%s.", String(temperature), sensorUnits[SENSOR_TEMPERATURE]);
        Serial.println();
      } else {
        Serial.println("Failed to read temperature from DHT sensor!");
      }
    }
    if (strcmp(sensorsSettings[i].address, "DHThumi") == 0) {
      if (!isnan(humidity)) {
        sensorsSettings[i].lastValue = humidity;
        sensorsSettings[i].lastUpdate = millis();
        sensorsSettings[i].present = true;
        sensorsSettings[i].type = SENSOR_HUMIDITY;
        strcpy(sensorsSettings[i].valueType, sensorUnits[SENSOR_HUMIDITY]);
        Serial.printf("    DHT humidity: %s%s.", String(humidity), sensorUnits[SENSOR_HUMIDITY]);
        Serial.println();
      } else {
        Serial.println("Failed to read humidity from DHT sensor!");
      }
    }
    if (strcmp(sensorsSettings[i].address, "DHTabsHumi") == 0) {
      if (!isnan(absoluteHumidity)) {
        sensorsSettings[i].lastValue = absoluteHumidity;
        sensorsSettings[i].lastUpdate = millis();
        sensorsSettings[i].present = true;
        sensorsSettings[i].type = SENSOR_ABSOLUTE_HUMIDITY;
        strcpy(sensorsSettings[i].valueType, sensorUnits[SENSOR_ABSOLUTE_HUMIDITY]);
        Serial.printf("    DHT absolute humidity: %s%s.", String(absoluteHumidity), sensorUnits[SENSOR_ABSOLUTE_HUMIDITY]);
        Serial.println();
      } else {
        Serial.println("Failed to read absolute humidity from DHT sensor!");
      }
    }
  }
}

float calculateAbsoluteHumidity(float temperatureC, float relativeHumidity) {
  return (6.112 * exp((17.67 * temperatureC) / (temperatureC + 243.5)) * relativeHumidity * 2.1674) / (273.15 + temperatureC);
};

void updateDS18B20Values() {
  sensors.requestTemperatures();
  uint8_t deviceCount = sensors.getDeviceCount();

  DeviceAddress addr;
  char addrStr[17];

  for (uint8_t i = 0; i < deviceCount; i++) {
    if (!sensors.getAddress(addr, i))
      continue;

    for (uint8_t j = 0; j < 8; j++)
      sprintf(&addrStr[j * 2], "%02X", addr[j]);
    addrStr[16] = '\0';
    int16_t idx = findSensorByAddress(addrStr);
    if (idx < 0) {
      Serial.println("-- sensor " + (String)addrStr + " nie w configu.");
      continue; // sensor nie w configu
    }

    float temp = sensors.getTempC(addr);

    if (temp == DEVICE_DISCONNECTED_C) {
      Serial.println(
          "-- Skipping disconnected (?) sensor_" + (String)addrStr + " [" + (String)sensorsSettings[idx].name + "] = " + (String)temp);
      continue;
    }

    sensorsSettings[idx].lastValue = temp;
    sensorsSettings[idx].lastUpdate = millis();
    sensorsSettings[idx].present = true;
    sensorsSettings[idx].type = SENSOR_TEMPERATURE;
    strcpy(sensorsSettings[idx].valueType, TYPE_CDEGREE);

    Serial.println("    sensor_" + (String)addrStr + " [" + (String)sensorsSettings[idx].name +
                   "] = " + (String)sensorsSettings[idx].lastValue + sensorUnits[SENSOR_TEMPERATURE] + ".");
  }
  Serial.println("   =" + String(deviceCount) + " DS18B20 sensor(s) requested.");
}

bool scanSensors() {
  DeviceAddress addr;
  bool configChanged = false;

  // reset present flags
  for (uint16_t i = 0; i < sensorsCount; i++) {
    sensorsSettings[i].present = false;
  }

  sensors.begin();
  delay(50); // mały czas dla OneWire

  uint8_t found = sensors.getDeviceCount();
  Serial.printf("   scanSensors: %u sensors found on OneWire", found);
  Serial.println();

  for (uint8_t i = 0; i < found; i++) {
    if (!sensors.getAddress(addr, i)) {
      Serial.printf("Failed to get address for sensor %u", i);
      Serial.println();
      continue;
    }

    char addrStr[17];
    for (uint8_t j = 0; j < 8; j++) {
      sprintf(&addrStr[j * 2], "%02X", addr[j]);
    }
    addrStr[16] = '\0';

    int16_t idx = findSensorByAddress(addrStr);
    if (idx >= 0) {
      sensorsSettings[idx].present = true;
      // Serial.printf("    Sensor present: %s", addrStr);
      // Serial.println();
    } else if (sensorsCount < MAX_SENSORS) {
      strlcpy(sensorsSettings[sensorsCount].address, addrStr, sizeof(sensorsSettings[sensorsCount].address));
      snprintf(sensorsSettings[sensorsCount].name, sizeof(sensorsSettings[sensorsCount].name), "Sensor %u", sensorsCount + 1);
      sensorsSettings[sensorsCount].compensation = 0.0f;
      sensorsSettings[sensorsCount].present = true;
      sensorsSettings[sensorsCount].type = SENSOR_TEMPERATURE;

      Serial.printf("New sensor added: %s", addrStr);
      Serial.println();
      sensorsCount++;
      configChanged = true;
    } else {
      Serial.println("Warning: MAX_SENSORS reached. Skipping sensor.");
    }
  }

  // ensure if DHT sensors are in the array
  const char *dhtAddresses[] = {"DHTtemp", "DHThumi", "DHTabsHumi"};
  const char *dhtNames[] = {"Temperatura DHT", "DHThumi", "DHT Abs Humidity"};
  for (uint8_t i = 0; i < sizeof(dhtAddresses) / sizeof(dhtAddresses[0]); i++) {
    if (findSensorByAddress(dhtAddresses[i]) < 0) {
      if (sensorsCount < MAX_SENSORS) {
        strlcpy(sensorsSettings[sensorsCount].address, dhtAddresses[i], sizeof(sensorsSettings[sensorsCount].address));
        strlcpy(sensorsSettings[sensorsCount].name, dhtNames[i], sizeof(sensorsSettings[sensorsCount].name));
        sensorsSettings[sensorsCount].compensation = 0.0f;
        sensorsSettings[sensorsCount].present = true; // DHT zawsze obecny
        sensorsCount++;
        configChanged = true;
        Serial.printf("Added DHT sensor: %s", dhtAddresses[i]);
        Serial.println();
      } else {
        Serial.println("Warning: MAX_SENSORS reached. Cannot add DHT sensor.");
      }
    } else {
      // if already existed, just mark as present
      sensorsSettings[findSensorByAddress(dhtAddresses[i])].present = true;
    }
  }

  if (configChanged) {
    saveConfig();
  }

  return true;
}

String formatAddressAsHex(DeviceAddress deviceAddress) {
  String address = "";
  uint8_t oneByte;
  char hexChars[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
  for (uint8_t i = 0; i < 8; i++) {
    oneByte = deviceAddress[i];
    address += hexChars[(oneByte & 0xF0) >> 4];
    address += hexChars[oneByte & 0x0F];
  }
  return address;
}

int16_t findSensorByAddress(const char *addr) {
  for (uint16_t i = 0; i < sensorsCount; i++) {
    if (strcmp(sensorsSettings[i].address, addr) == 0)
      return i;
  }
  return -1;
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
