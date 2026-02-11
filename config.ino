/*
   filesystem
*/
#include <LittleFS.h>
const char *fsName = "LittleFS";
FS *fileSystem = &LittleFS;
LittleFSConfig fileSystemConfig = LittleFSConfig();
static bool fsOK;

#define CONFIG_FILE "/config.json"

#include <ArduinoJson.h>

bool loadConfig() {
  if (!LittleFS.exists(CONFIG_FILE)) {
    Serial.println("Config not found");
    sensorsCount = 0;
    return false;
  }

  File f = LittleFS.open(CONFIG_FILE, "r");
  if (!f)
    return false;

  DynamicJsonDocument doc(8192);
  auto err = deserializeJson(doc, f);
  f.close();

  if (err) {
    Serial.print("JSON parse error: ");
    Serial.println(err.c_str());

    return false;
  }

  JsonArray arr = doc["sensors"];
  if (!arr)
    return false;

  sensorsCount = 0;

  for (JsonObject o : arr) {
    if (sensorsCount >= MAX_SENSORS)
      break;

    strlcpy(sensorsSettings[sensorsCount].address, o["address"] | "", sizeof(sensorsSettings[sensorsCount].address));
    strlcpy(sensorsSettings[sensorsCount].name, o["name"] | "Sensor", sizeof(sensorsSettings[sensorsCount].name));
    sensorsSettings[sensorsCount].compensation = o["compensation"] | 0.0f;
    sensorsSettings[sensorsCount].type = (SensorType)(o["type"] | SENSOR_UNKNOWN);
    sensorsSettings[sensorsCount].present = false;
    sensorsCount++;
  }

  Serial.printf("Loaded %u sensors from config", sensorsCount);
  Serial.println();

  // temperature compensation for ccs811
  config.compTemp.method = (ImportMethod)doc["compT"]["m"].as<int>();
  config.compTemp.source = doc["compT"]["src"] | "";
  strlcpy(config.compTemp.address, doc["compT"]["addr"] | "", sizeof(config.compTemp.address));

  // humidity compensation for ccs811
  config.compHumi.method = (ImportMethod)doc["compH"]["m"].as<int>();
  config.compHumi.source = doc["compH"]["src"] | "";
  strlcpy(config.compHumi.address, doc["compH"]["addr"] | "", sizeof(config.compHumi.address));

  return true;
}

bool saveConfig() {
  if (sensorsCount == 0) {
    Serial.println("No sensors to save.");
    return false;
  }

  DynamicJsonDocument doc(8192);

  JsonObject root = doc.to<JsonObject>();
  root["version"] = programVersion;

  JsonArray sensorsArray = root.createNestedArray("sensors");

  for (uint16_t i = 0; i < sensorsCount; i++) {
    JsonObject o = sensorsArray.createNestedObject();
    o["address"] = sensorsSettings[i].address;
    o["name"] = sensorsSettings[i].name;
    o["compensation"] = sensorsSettings[i].compensation;
    o["type"] = (uint8_t)sensorsSettings[i].type;
  }

  JsonObject ct = doc.createNestedObject("compT");
  ct["m"] = (int)config.compTemp.method;
  ct["src"] = config.compTemp.source;
  ct["addr"] = config.compTemp.address;

  JsonObject ch = doc.createNestedObject("compH");
  ch["m"] = (int)config.compHumi.method;
  ch["src"] = config.compHumi.source;
  ch["addr"] = config.compHumi.address;

  File f = LittleFS.open(CONFIG_FILE, "w");
  if (!f) {
    Serial.println("Cannot open config file for writing!");
    return false;
  }

  if (serializeJsonPretty(root, f) == 0) {
    Serial.println("JSON serialization failed!");
    f.close();
    return false;
  }

  f.close();
  Serial.println("Config saved successfully!");
  return true;
}
