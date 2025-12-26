#define RESET_PIN 0  // NodeMCU D3

#define AP_THRESHOLD 2000        // 2s → AP mode
#define WIFI_RESET_THRESHOLD 5000 // 5s → WiFi reset
#define FULL_RESET_THRESHOLD 10000 // 10s → full factory reset

unsigned long pressStart = 0;
bool resetPressed = false;

void initResetButton() {
  pinMode(RESET_PIN, INPUT_PULLUP);  // assuming button shorts to GND
}

void checkResetButton() {
  int state = digitalRead(RESET_PIN);

  if (state == LOW && !resetPressed) { // button pressed
    pressStart = millis();
    resetPressed = true;
  }

  if (state == HIGH && resetPressed) { // button released
    unsigned long duration = millis() - pressStart;
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
    }

    resetPressed = false;
  }

  if (resetPressed) {
    unsigned long duration = millis() - pressStart;
    Serial.print("\rHolding button: ");
    Serial.print(duration / 1000.0, 2);
    Serial.print(" s   ");
  }

  delay(50); // lekki debounce
}

void fullFactoryReset() {
  if (!LittleFS.begin()) return;
  
  // usuwamy wszystkie pliki konfiguracyjne
  if (LittleFS.exists("/wifi.json")) LittleFS.remove("/wifi.json");
  if (LittleFS.exists("/config.json")) LittleFS.remove("/config.json");
  
  Serial.println("All config files removed. Restarting...");
  ESP.restart();
}

