// wifi.ino

#include <ESP8266NetBIOS.h>

void initWiFi() {
  bool connected = false;

  Serial.println("Loading WiFi config...");
  if (loadWiFiConfig()) {
    Serial.println("trying to start as a station...");
    connected = startSTAMode();
  }

  if (!connected) {
    Serial.println("naah, let's be an AP.");
    startAPMode();
  }
}

// --- AP mode start ---
void startAPMode() {
  Serial.println("Starting AP mode...");
  String apSSID = "kbSensors-" + String((uint32_t)(ESP.getChipId() & 0xFFFF), HEX);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID.c_str(), "12345678"); // default password
  Serial.println("AP started: " + (String)apSSID);
}

// --- STA mode start ---
bool startSTAMode() {
  Serial.println("Trying to start STA mode...");
  if (wifiSSID.length() == 0) {
    Serial.println("wifiSSID.length()=0.");
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
  Serial.printf("Connecting to WiFi: %s ...", wifiSSID.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("Connected! IP: %s", WiFi.localIP().toString().c_str());
    Serial.println();
    return true;
  } else {
    Serial.println("Failed to connect to WiFi");
    return false;
  }
}

void initNetbiosName() { NBNS.begin(NETBIOSNAME); }

bool loadWiFiConfig() {
  if (!LittleFS.begin()) {
    Serial.println("FS init failed");
    return false;
  }

  if (!LittleFS.exists(WIFI_CONFIG_FILE)) {
    Serial.println("WiFi config not found");
    return false;
  }

  File f = LittleFS.open(WIFI_CONFIG_FILE, "r");
  if (!f) {
    Serial.println("Failed to open wifi.json");
    return false;
  }

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, f);
  f.close();

  if (error) {
    Serial.println("Failed to parse wifi.json");
    return false;
  }

  wifiSSID = doc["ssid"] | "";
  wifiPassword = doc["wifipassword"] | "";
  webUser = doc["webuser"] | "admin";
  webPass = doc["webpass"] | "admin";

  Serial.printf("Loaded WiFi: %s", wifiSSID.c_str());
  Serial.println();
  return true;
}

bool saveWiFiConfig() {

  StaticJsonDocument<256> doc;
  doc["ssid"] = wifiSSID;
  doc["wifipassword"] = wifiPassword;
  doc["webuser"] = webUser;
  doc["webpass"] = webPass;

  File f = LittleFS.open(WIFI_CONFIG_FILE, "w");
  if (!f) {
    Serial.println("Failed to open wifi.json for writing");
    return false;
  }

  if (serializeJson(doc, f) == 0) {
    Serial.println("Failed to write wifi.json");
    f.close();
    return false;
  }

  f.close();
  Serial.println("WiFi config saved");
  return true;
}

void resetWiFiConfig() {
  if (LittleFS.exists(WIFI_CONFIG_FILE)) {
    LittleFS.remove(WIFI_CONFIG_FILE);
    Serial.println("wifi.json deleted. WiFi configuration reset!");
  }
}
