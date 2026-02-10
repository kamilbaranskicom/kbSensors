unsigned long lastScanTime = 0;
unsigned long currentWaitInterval = 60000;    // Current wait interval (can be changed by force update)
const unsigned long SCAN_INTERVAL_MS = 60000; // 60 seconds
// the sensors are slow; you can read them not more often than 1s.
const unsigned long FLOOD_GUARD_MS = 1000; // 1 second

bool sensorAdded;

void initSensors() {
  bool sensorAdded = false;

  ds18b20_init();
  dht11_init();
  ccs811_init();

  if (sensorAdded) {
    saveConfig();
    publishAllHADiscovery();
  }
  scanSensors();

  registerForceUpdate(0);
}

void scanSensors() {
  // reset present flags
  for (uint16_t i = 0; i < sensorsCount; i++) {
    sensorsSettings[i].present = false;
  }

  bool sensorAdded = false;

  // Wykorzystujemy bitowy OR (|), aby każda funkcja się wykonała
  ds18b20_update();
  dht11_update();
  ccs811_update();

  if (sensorAdded) { // new sensor
    saveConfig();
    publishAllHADiscovery();
  }
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

    // check if any new sensors appeared or disappeared and update their values
    scanSensors();

    // 3. Po wykonaniu odczytu:
    lastScanTime = millis();                // Resetujemy czas bazowy
    currentWaitInterval = SCAN_INTERVAL_MS; // Przywracamy domyślną minutę

    Serial.println("Scan finished. Next regular scan in 60s.");
    mqttPublishChangedSensors();
  }
}

/**
 * Główna funkcja przyjmująca odczyt z dowolnego czujnika.
 * Realizuje logikę "Upsert":
 * 1. Szuka sensora po adresie.
 * 2. Jeśli nie ma - rejestruje go.
 * 3. Aplikuje kompensację.
 * 4. Aktualizuje wartość i czas.
 */
bool submitSensorReading(const char *address, float rawValue, SensorType type, const char *defaultName) {
  // 1. Walidacja
  if (isnan(rawValue))
    return false;

  int16_t idx = findSensorByAddress(address);

  // 2. Jeśli nie znaleziono - próbujemy dodać (Auto-Discovery)
  if (idx == -1) {
    if (sensorsCount < MAX_SENSORS) {
      idx = sensorsCount; // Nowy indeks
      strlcpy(sensorsSettings[idx].address, address, sizeof(sensorsSettings[idx].address));
      strlcpy(sensorsSettings[idx].name, defaultName, sizeof(sensorsSettings[idx].name));
      sensorsSettings[idx].compensation = 0.0f;
      sensorsSettings[idx].type = type; // Ustawiamy typ przy pierwszym znalezieniu
      sensorsCount++;

      Serial.printf("New sensor detected and added: %s (%s)\n", defaultName, address);
      sensorAdded = true; // global flag to indicate a new sensor was added
    } else {
      Serial.println("Warning: MAX_SENSORS reached. Cannot add new sensor.");
      return false;
    }
  }

  // 3. Aktualizacja danych (dla istniejącego lub właśnie dodanego)
  // Upewniamy się, że typ jest aktualny (na wypadek gdybyś zmienił definicję w kodzie)
  if (sensorsSettings[idx].type == SENSOR_UNKNOWN) {
    sensorsSettings[idx].type = type;
  }

  // we add compensation later in the pipeline, so we store raw value here
  sensorsSettings[idx].lastValue = rawValue;
  sensorsSettings[idx].lastUpdate = millis();
  sensorsSettings[idx].present = true;

  // 4. Wspólne logowanie
  Serial.printf("    %s [%s]: raw %.2f%s; comp %.2f%s\n",
      sensorsSettings[idx].address,
      sensorsSettings[idx].name,
      rawValue,
      getSensorUnits(sensorsSettings[idx]).c_str(),
      rawValue + sensorsSettings[idx].compensation,
      getSensorUnits(sensorsSettings[idx]).c_str());
  return true;
}

int16_t findSensorByAddress(const char *addr) {
  for (uint16_t i = 0; i < sensorsCount; i++) {
    if (strcmp(sensorsSettings[i].address, addr) == 0)
      return i;
  }
  return -1;
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