#define RESET_PIN 0  // NodeMCU D3

#define AP_THRESHOLD 2000        // 2s → AP mode
#define WIFI_RESET_THRESHOLD 5000 // 5s → WiFi reset
#define FULL_RESET_THRESHOLD 10000 // 10s → full factory reset

unsigned long pressStart = 0;
bool resetPressed = false;
unsigned long lastButtonCheck = 0;
unsigned long lastHoldReport = 0;

void initResetButton() {
  pinMode(RESET_PIN, INPUT_PULLUP);  // assuming button shorts to GND
}

void checkResetButton() {
  unsigned long now = millis();
  if (now - lastButtonCheck < 50) // debounce without blocking loop() (web server, OTA...)
    return;
  lastButtonCheck = now;

  int state = digitalRead(RESET_PIN);

  if (state == LOW && !resetPressed) { // button pressed
    pressStart = now;
    lastHoldReport = now;
    resetPressed = true;
  }

  if (state == HIGH && resetPressed) { // button released
    unsigned long duration = now - pressStart;
    resetPressed = false;
    Serial.print("\rButton released after ");
    Serial.print(duration / 1000.0, 2);
    Serial.println(" s        ");

    if (duration >= FULL_RESET_THRESHOLD) {
      Serial.println("Full factory reset triggered!");
      fullFactoryReset();
    } else if (duration >= WIFI_RESET_THRESHOLD) {
      Serial.println("WiFi reset triggered!");
      resetWiFiConfig(); // usuwa wifi.json i ustawia domyślne dane
      ESP.restart();
    } else if (duration >= AP_THRESHOLD) {
      Serial.println("AP mode triggered!");
      startAPMode();
      initWebserver(); // switch to the captive-portal routes
    }
    return;
  }

  if (resetPressed && now - lastHoldReport >= 500) {
    lastHoldReport = now;
    Serial.print("\rHolding button: ");
    Serial.print((now - pressStart) / 1000.0, 1);
    Serial.print(" s   ");
  }
}

void fullFactoryReset() {
  if (!LittleFS.begin()) return;
  
  // usuwamy wszystkie pliki konfiguracyjne
  if (LittleFS.exists("/wifi.json")) LittleFS.remove("/wifi.json");
  if (LittleFS.exists("/config.json")) LittleFS.remove("/config.json");
  if (LittleFS.exists("/mqtt.json")) LittleFS.remove("/mqtt.json");
  
  Serial.println("All config files removed. Restarting...");
  ESP.restart();
}
