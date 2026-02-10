unsigned long lastScanTime = 0;
unsigned long currentWaitInterval = 60000;    // Current wait interval (can be changed by force update)
const unsigned long SCAN_INTERVAL_MS = 60000; // 60 seconds
// the sensors are slow; you can read them not more often than 1s.
const unsigned long FLOOD_GUARD_MS = 1000; // 1 second

void initSensors() {
  // Reconfiguring I2C bus to avoid Flash pin conflicts
  Wire.begin(I2C_SDA, I2C_SCL);

  scanSensors();
  registerForceUpdate(0);
}

bool scanSensors() {
  // reset present flags
  for (uint16_t i = 0; i < sensorsCount; i++) {
    sensorsSettings[i].present = false;
  }

  // Wykorzystujemy bitowy OR (|), aby każda funkcja się wykonała
  bool changed = ds18b20_scan() | dht_scan() | ccs811_scan();

  if (changed) {
    saveConfig();
    publishAllHADiscovery();
  }

  return changed;
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

void handleSensors() {
  unsigned long now = millis();
  if (now - lastScanTime < FLOOD_GUARD_MS)
    return;

  // 2. Sprawdzamy, czy upłynął zaplanowany czas (zwykły lub wymuszony)
  if (now - lastScanTime >= currentWaitInterval) {
    Serial.println("Executing sensor read...");
    // check if any new sensors appeared or disappeared:
    scanSensors();

    // read values from sensors and update lastValue/lastUpdate:
    dht_update();
    ds18b20_update();
    ccs811_update();

    // 3. Po wykonaniu odczytu:
    lastScanTime = millis();                // Resetujemy czas bazowy
    currentWaitInterval = SCAN_INTERVAL_MS; // Przywracamy domyślną minutę

    Serial.println("Scan finished. Next regular scan in 60s.");
    mqttPublishChangedSensors();
  }
}

bool registerSensor(const char *address, const char *defaultName, SensorType type) {
  int16_t idx = findSensorByAddress(address);

  // exists
  if (idx != -1) {
    sensorsSettings[idx].type = type; // update type if we haven't set it already (i.e. when we have just loaded config.json)
    sensorsSettings[idx].present = true;
    return false;
  }

  // new sensor
  if (sensorsCount < MAX_SENSORS) {
    strlcpy(sensorsSettings[sensorsCount].address, address, sizeof(sensorsSettings[sensorsCount].address));
    strlcpy(sensorsSettings[sensorsCount].name, defaultName, sizeof(sensorsSettings[sensorsCount].name));
    sensorsSettings[sensorsCount].type = type;
    sensorsSettings[sensorsCount].compensation = 0.0f;
    sensorsSettings[sensorsCount].present = true;

    Serial.printf("New sensor registered: %s (%s)\n", defaultName, address);
    sensorsCount++;
    return true;
  }

  Serial.println("Warning: MAX_SENSORS reached!");
  return false;
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

String getSensorType(const SensorConfig &s) {
  if (s.type < sizeof(sensorTypeStrs) / sizeof(sensorTypeStrs[0])) {
    return sensorTypeStrs[s.type];
  }
  return sensorTypeStrs[SENSOR_UNKNOWN];
}

String getSensorUnits(const SensorConfig &s) {
  if (s.type < sizeof(sensorUnits) / sizeof(sensorUnits[0])) {
    return sensorUnits[s.type];
  }
  return sensorUnits[SENSOR_UNKNOWN];
}

String trimmed(String s) {
  s.trim();
  return s;
}