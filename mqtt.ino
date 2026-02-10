#include <PubSubClient.h>
WiFiClient espClient;
PubSubClient mqttClient(espClient);

struct MQTTConfig {
  String host = "192.168.50.13";  // MQTT broker address
  uint16_t port = 1883;           // MQTT port
  String user = "kbSensors";      // username
  String pass = "uEYxcbuPc6ufKL"; // password
  String baseTopic = "kbSensors"; // topic base
  bool enabled = true;            // enable/disable MQTT
  uint8_t qos = 0;                // QoS, default 0
  bool retain = true;             // retain flag
};

MQTTConfig mqtt;
String mqttDeviceId;
WiFiClient wifiClient;

unsigned long lastMqttReconnectAttempt = 0;
bool haDiscoverySent = false;

void initMQTT() {
  mqttDeviceId = "kbsensors_" + String(ESP.getChipId(), HEX);

  mqttClient.setServer(mqtt.host.c_str(), mqtt.port);
  mqttClient.setBufferSize(1024);

  Serial.print(F("[MQTT] Device ID: "));
  Serial.println(mqttDeviceId);
  Serial.printf("(MQTT_MAX_PACKET_SIZE=%d)", MQTT_MAX_PACKET_SIZE);
  Serial.println();
}

bool mqttConnect() {
  if (!mqtt.enabled)
    return false;
  if (mqttClient.connected())
    return true;

  Serial.print(F("[MQTT] Connecting... "));

  bool ok;
  if (mqtt.user.length()) {
    ok = mqttClient.connect(mqttDeviceId.c_str(),
        mqtt.user.c_str(),
        mqtt.pass.c_str(),
        (mqtt.baseTopic + "/" + mqttDeviceId + "/status").c_str(),
        mqtt.qos,
        true,
        "offline");
  } else {
    ok = mqttClient.connect(mqttDeviceId.c_str());
  }

  if (ok) {
    Serial.println(F("OK"));
    mqttPublishStatus("online");
    // publishAllHADiscovery(); // nie, bo za pierwszym razem jeszcze nie ma wszystkiego.
  } else {
    Serial.print(F("FAILED rc="));
    Serial.println(mqttClient.state());
  }

  return ok;
}

void mqttPublishStatus(const char *state) {
  if (!mqtt.enabled)
    return;
  if (!mqttClient.connected())
    return;

  String topic = mqtt.baseTopic + "/" + mqttDeviceId + "/status";

  String payload = String("{\"state\":\"") + state + "\",\"ip\":\"" + WiFi.localIP().toString() + "\",\"uptime\":" + millis() / 1000 + "}";

  mqttClient.publish(topic.c_str(), payload.c_str(), mqtt.retain);
}

void mqttLoop() {
  if (!mqtt.enabled)
    return;

  if (!mqttClient.connected()) {
    haDiscoverySent = false; // Resetujemy flagę przy rozłączeniu
    unsigned long now = millis();
    if (now - lastMqttReconnectAttempt > 5000) {
      lastMqttReconnectAttempt = now;
      if (mqttConnect()) {
        // Opcja A: wysyłamy od razu po udanym connect
        publishAllHADiscovery();
        haDiscoverySent = true;
      }
    }
    return;
  }

  // Opcja B: wysyłamy tutaj, jeśli połączenie jest, a flaga jeszcze nie puszczona
  if (!haDiscoverySent) {
    publishAllHADiscovery();
    haDiscoverySent = true;
  }

  mqttClient.loop();
}

#include <ArduinoJson.h>
#include <LittleFS.h>

// ---------------------------
// JSON file helpers
// ---------------------------
#define MQTT_CONFIG_FILE "/mqtt.json"

bool loadMQTTConfig() {
  if (!LittleFS.begin()) {
    Serial.println("Failed to mount FS for MQTT config");
    return false;
  }

  if (!LittleFS.exists(MQTT_CONFIG_FILE)) {
    Serial.println("MQTT config not found, using defaults");
    return false;
  }

  File f = LittleFS.open(MQTT_CONFIG_FILE, "r");
  if (!f) {
    Serial.println("Failed to open MQTT config for reading");
    return false;
  }

  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    Serial.println("Failed to parse MQTT config, using defaults");
    return false;
  }

  mqtt.host = doc["host"] | mqtt.host;
  mqtt.port = doc["port"] | mqtt.port;
  mqtt.user = doc["user"] | mqtt.user;
  mqtt.pass = doc["pass"] | mqtt.pass;
  mqtt.baseTopic = doc["baseTopic"] | mqtt.baseTopic;
  mqtt.enabled = doc["enabled"] | mqtt.enabled;
  mqtt.qos = doc["qos"] | mqtt.qos;
  mqtt.retain = doc["retain"] | mqtt.retain;

  Serial.printf("MQTT config loaded: %s:%d (%s)", mqtt.host.c_str(), mqtt.port, mqttDeviceId.c_str());
  Serial.println();
  return true;
}

bool saveMQTTConfig() {
  File f = LittleFS.open(MQTT_CONFIG_FILE, "w");
  if (!f) {
    Serial.println("Failed to open MQTT config for writing");
    return false;
  }

  StaticJsonDocument<512> doc;
  doc["host"] = mqtt.host;
  doc["port"] = mqtt.port;
  doc["user"] = mqtt.user;
  doc["pass"] = mqtt.pass;
  doc["baseTopic"] = mqtt.baseTopic;
  doc["enabled"] = mqtt.enabled;
  doc["qos"] = mqtt.qos;
  doc["retain"] = mqtt.retain;

  if (serializeJson(doc, f) == 0) {
    Serial.println("ERROR: mqtt.json write failed");
    f.close();
    return false;
  }
  f.close();

  Serial.println("MQTT config saved");
  return true;
}

void mqttPublishChangedSensors(bool forceAll) {
  for (int i = 0; i < MAX_SENSORS; i++) {
    SensorConfig &s = sensorsSettings[i];

    if (!s.present)
      continue; // ignoruj brakujące czujniki

    float correctedValue = s.lastValue + s.compensation;
    if (forceAll || fabs(correctedValue - s.lastPublishedValue) >= 0.01) { // próg zmiany
      mqttPublishSensor(s);
    }
  }
}

void mqttPublishChangedSensors() { mqttPublishChangedSensors(false); }

void mqttPublishSensor(const SensorConfig &s) {
  if (!mqttClient.connected())
    return;

  float correctedValue = s.lastValue + s.compensation;

  const char *typeStr = "unknown";
  const char *unitStr = "";

  String topic = mqtt.baseTopic + "/" + mqttDeviceId + "/sensor/" + getSensorType(s) + "/" + s.address;

  StaticJsonDocument<384> doc;
  doc["address"] = s.address;
  doc["name"] = s.name;
  doc["valueType"] = getSensorType(s);
  doc["value"] = correctedValue;
  doc["unit"] = trimmed(getSensorUnits(s));
  doc["lastUpdate"] = s.lastUpdate;

  char payload[384];
  size_t n = serializeJson(doc, payload, sizeof(payload));

  mqttClient.publish(topic.c_str(), payload, mqtt.retain);

  // zapamiętanie, że już wysłano
  const_cast<SensorConfig &>(s).lastPublishedValue = correctedValue;
}

void mqttPublishHADiscovery(const SensorConfig &s) {
  if (!mqttClient.connected())
    return;

  String typeStr = getSensorType(s);
  String unitStr = trimmed(getSensorUnits(s));

  // HA component (na razie wszystko jako sensor)
  const char *component = "sensor";

  String uniqueId = mqttDeviceId + "_" + typeStr + "_" + s.address;
  String discoveryTopic = "homeassistant/" + String(component) + "/" + uniqueId + "/config";
  String stateTopic = mqtt.baseTopic + "/" + mqttDeviceId + "/sensor/" + typeStr + "/" + s.address;

  DynamicJsonDocument doc(1024);

  doc["name"] = String(s.name);
  doc["unique_id"] = uniqueId;
  doc["state_topic"] = stateTopic;
  doc["device_class"] = typeStr;
  doc["unit_of_measurement"] = unitStr;
  doc["value_template"] = "{{ value_json.value }}";
  doc["availability_topic"] = mqtt.baseTopic + "/" + mqttDeviceId + "/status";
  doc["availability_template"] = "{{ value_json.state }}";
  doc["payload_available"] = "online";
  doc["payload_not_available"] = "offline";

  // wspólne urządzenie
  JsonObject device = doc.createNestedObject("device");
  device["identifiers"][0] = mqttDeviceId;
  device["name"] = deviceName;
  device["model"] = "kbSensors";
  device["manufacturer"] = "Kamil Baranski";

  char payload[1024];
  serializeJson(doc, payload, sizeof(payload));

  // debug
  // Serial.println("[MQTT] DEBUG Discovery Payload:");
  // Serial.println(payload); // Zobacz, czy JSON nie jest ucięty
  bool result = mqttClient.publish(discoveryTopic.c_str(), payload, true);
  Serial.printf("[MQTT] %s HA discovery publish result: %s", s.address, result ? "OK" : "FAILED (Check Buffer Size)");
  Serial.println();
}

void publishAllHADiscovery() {
  for (int i = 0; i < MAX_SENSORS; i++) {
    if (sensorsSettings[i].present) {
      mqttPublishHADiscovery(sensorsSettings[i]);
    }
  }
}
