#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

unsigned long lastImportTime = 0;
unsigned long currentImportWaitInterval = 60000;
const unsigned long IMPORT_FLOOD_GUARD_MS = 5000; // 5 seconds
const unsigned long IMPORT_INTERVAL_MS = 60000;   // 60 seconds

// Funkcja pomocnicza: Wyciąga wartość dla konkretnego adresu z JSON-a kbSensors
float findValueInJson(const String &jsonPayload, const char *targetAddress) {
  // Optymalizacja: Filtrujemy JSON, żeby nie alokować pamięci na nazwy, typy itp.
  // Interesuje nas tylko tablica sensors[].address i sensors[].value
  StaticJsonDocument<200> filter;
  filter["sensors"][0]["address"] = true;
  filter["sensors"][0]["value"] = true;

  // Alokujemy trochę więcej, bo tablica może być długa
  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, jsonPayload, DeserializationOption::Filter(filter));

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return NAN;
  }

  JsonArray sensors = doc["sensors"];
  for (JsonObject s : sensors) {
    const char *addr = s["address"];
    if (addr && strcmp(addr, targetAddress) == 0) {
      return s["value"].as<float>();
    }
  }
  return NAN;
}

// Helper do wyciągania wartości z lokalnej tablicy
float getLocalSensorValue(const char *address) {
  int16_t idx = findSensorByAddress(address);
  if (idx != -1 && sensorsSettings[idx].present) {
    // Zwracamy wartość skompensowaną (bo takiej używamy w JSON/MQTT)
    return sensorsSettings[idx].lastValue + sensorsSettings[idx].compensation;
  }
  return NAN;
}

// Funkcja do pobierania HTTP (polling co np. 60s)
void handleImports() {
  unsigned long now = millis();
  if (now - lastImportTime < IMPORT_FLOOD_GUARD_MS)
    return;

  if (now - lastImportTime < currentImportWaitInterval) {
    return;
  }

  // 1. Import Temperatury po HTTP
  if (config.compTemp.method == IMPORT_LOCAL) {
    float val = getLocalSensorValue(config.compTemp.address);
    if (!isnan(val)) {
      globalEnv.temperature = val;
      globalEnv.lastTempUpdate = millis();
      // Debug tylko przy zmianie lub rzadziej, żeby nie śmiecić
      // Serial.printf("[Import] Local Temp: %.2f\n", val);
    }
  } else if (config.compTemp.method == IMPORT_HTTP_JSON) {
    Serial.println("temp JSON");
    float val = fetchHttpValue(config.compTemp.source, config.compTemp.address);
    if (!isnan(val)) {
      globalEnv.temperature = val;
      globalEnv.lastTempUpdate = millis();
      Serial.printf("[Import] Temp updated from HTTP: %.2f\r\n", val);
    }
  }

  // 2. Import Wilgotności po HTTP
  if (config.compHumi.method == IMPORT_LOCAL) {
    float val = getLocalSensorValue(config.compHumi.address);
    if (!isnan(val)) {
      globalEnv.humidity = val;
      globalEnv.lastHumiUpdate = millis();
    }
  } else if (config.compHumi.method == IMPORT_HTTP_JSON) {
    Serial.println("humi JSON");
    // Jeśli URL jest ten sam co dla Temp, warto by to zoptymalizować i pobrać raz,
    // ale dla czytelności zostawmy oddzielnie (ESP sobie poradzi z 2 requestami na minutę).
    float val = fetchHttpValue(config.compHumi.source, config.compHumi.address);
    if (!isnan(val)) {
      globalEnv.humidity = val;
      globalEnv.lastHumiUpdate = millis();
      Serial.printf("[Import] Humi updated from HTTP: %.2f\r\n", val);
    }
  }

  lastImportTime = millis();
  Serial.println("Import finished. Next regular import in 60s.");
}

void registerForceImportUpdate(int intervalSeconds) {
  unsigned long now = millis();
  unsigned long elapsed = now - lastImportTime; // Ile czasu już minęło od ostatniego razu
  unsigned long requestedTotalInterval = elapsed + (intervalSeconds * 1000ULL);

  // Jeśli nowy całkowity czas oczekiwania jest krótszy niż ten, który mamy zaplanowany...
  if (requestedTotalInterval < currentImportWaitInterval) {
    currentImportWaitInterval = requestedTotalInterval;

    // Zabezpieczenie: interwał nie może być krótszy niż flood guard
    if (currentImportWaitInterval < IMPORT_FLOOD_GUARD_MS) {
      currentImportWaitInterval = IMPORT_FLOOD_GUARD_MS;
    }

    Serial.printf("New interval set: trigger in %ds", intervalSeconds);
    Serial.println();
  } else {
    Serial.println("Force update ignored - existing schedule is sooner.");
  }
}

float fetchHttpValue(String url, const char *sensorAddr) {
  WiFiClient client;
  HTTPClient http;
  float result = NAN;

  Serial.printf("[Import] Fetching %s looking for %s...\r\n", url.c_str(), sensorAddr);

  if (http.begin(client, url)) {
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      result = findValueInJson(http.getString(), sensorAddr);
    } else {
      Serial.printf("[Import] HTTP Error: %d\r\n", httpCode);
    }
    http.end();
  } else {
    Serial.println("[Import] HTTP begin failed.");
  }
  return result;
}

// Funkcja wywoływana z MQTT callback
void processMqttImport(String topic, byte *payload, unsigned int length) {
  // Zakładamy, że payload to JSON: {"value": 12.3, ...} (format kbSensors MQTT)
  // Lub po prostu liczba (zależy jak skonfigurujesz).
  // Ale skoro integrujemy kbSensors <-> kbSensors, to format MQTT jest taki jak w mqttPublishSensor.

  StaticJsonDocument<384> doc;
  deserializeJson(doc, payload, length);
  float val = doc["value"] | NAN; // Jeśli brak pola value, zwróci NAN

  if (isnan(val))
    return;

  if ((config.compTemp.method == IMPORT_MQTT_JSON) && (topic == config.compTemp.source)) {
    globalEnv.temperature = val;
    globalEnv.lastTempUpdate = millis();
    Serial.printf("[Import] Temp updated from MQTT: %.2f\r\n", val);
  }

  if ((config.compHumi.method == IMPORT_MQTT_JSON) && (topic == config.compHumi.source)) {
    globalEnv.humidity = val;
    globalEnv.lastHumiUpdate = millis();
    Serial.printf("[Import] Humi updated from MQTT: %.2f\r\n", val);
  }
}