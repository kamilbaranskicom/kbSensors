
#include <PubSubClient.h>
WiFiClient espClient;
PubSubClient mqttClient(espClient);

struct MQTTConfig {
  String host = "192.168.50.13";   // MQTT broker address
  uint16_t port = 1883;            // MQTT port
  String user = "kbSensors";       // username
  String pass = "uEYxcbuPc6ufKL";  // password
  String baseTopic = "kbSensors";  // topic base
  bool enabled = true;             // enable/disable MQTT
  uint8_t qos = 0;                 // QoS, default 0
  bool retain = true;              // retain flag
};

MQTTConfig mqtt;
String mqttDeviceId;
WiFiClient wifiClient;


unsigned long lastMqttReconnectAttempt = 0;



void initMQTT() {
  mqttDeviceId = "kbsensors_" + String(ESP.getChipId(), HEX);

  mqttClient.setServer(mqtt.host.c_str(), mqtt.port);

  Serial.print(F("[MQTT] Device ID: "));
  Serial.println(mqttDeviceId);
  Serial.printf("(MQTT_MAX_PACKET_SIZE=%d)\n", MQTT_MAX_PACKET_SIZE);
}


bool mqttConnect() {
  if (!mqtt.enabled) return false;
  if (mqttClient.connected()) return true;

  Serial.print(F("[MQTT] Connecting... "));

  bool ok;
  if (mqtt.user.length()) {
    ok = mqttClient.connect(
      mqttDeviceId.c_str(),
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
  } else {
    Serial.print(F("FAILED rc="));
    Serial.println(mqttClient.state());
  }

  return ok;
}

void mqttPublishStatus(const char *state) {
  if (!mqtt.enabled) return;
  if (!mqttClient.connected()) return;

  String topic = mqtt.baseTopic + "/" + mqttDeviceId + "/status";

  String payload =
    String("{\"state\":\"") + state + "\",\"ip\":\"" + WiFi.localIP().toString() + "\",\"uptime\":" + millis() / 1000 + "}";

  mqttClient.publish(
    topic.c_str(),
    payload.c_str(),
    mqtt.retain);
}

void mqttLoop() {
  if (!mqtt.enabled) return;

  if (!mqttClient.connected()) {
    unsigned long now = millis();
    if (now - lastMqttReconnectAttempt > 5000) {
      lastMqttReconnectAttempt = now;
      mqttConnect();
    }
    return;
  }

  mqttClient.loop();
}

#include <LittleFS.h>
#include <ArduinoJson.h>

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

  Serial.printf("MQTT config loaded: %s:%d (%s)\n", mqtt.host.c_str(), mqtt.port, mqttDeviceId.c_str());
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


void mqttPublishChangedSensors() {
  for (int i = 0; i < MAX_SENSORS; i++) {
    SensorConfig &s = sensorsSettings[i];

    if (!s.present) continue;  // ignoruj brakujące czujniki

    float correctedValue = s.lastValue + s.compensation;
    if (fabs(correctedValue - s.lastPublishedValue) >= 0.01) {  // próg zmiany
      mqttPublishSensor(s);
    }
  }
}



void mqttPublishSensor(const SensorConfig &s) {
  if (!mqttClient.connected()) return;

  float correctedValue = s.lastValue + s.compensation;

  const char *typeStr = "unknown";
  const char *unitStr = "";
  if (s.type < sizeof(sensorTypeStrs) / sizeof(sensorTypeStrs[0])) {
    typeStr = sensorTypeStrs[s.type];
    unitStr = sensorUnits[s.type];
  }

  String topic =
    mqtt.baseTopic + "/" + mqttDeviceId + "/sensor/" + String(typeStr) + "/" + s.address;

  StaticJsonDocument<256> doc;
  doc["address"] = s.address;
  doc["name"] = s.name;
  doc["valueType"] = s.valueType;
  doc["value"] = correctedValue;
  doc["unit"] = unitStr;
  doc["lastUpdate"] = s.lastUpdate;

  char payload[256];
  size_t n = serializeJson(doc, payload, sizeof(payload));

  mqttClient.publish(topic.c_str(), payload, mqtt.retain);

  // zapamiętanie, że już wysłano
  const_cast<SensorConfig &>(s).lastPublishedValue = correctedValue;
}
