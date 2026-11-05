void initSerial() {
  Serial.begin(115200);
  Serial.setTimeout(50);
  showAbout();
  Serial.println("Serial initialized!");
}

void showAbout() {
  Serial.println("\n\n");
  Serial.println(programName);
  Serial.println(programVersion);
  Serial.println();
  Serial.println(programManual);
  Serial.println("\n\n");
}

#define SERIAL_BUF_SIZE 128 // SSID (32) + password (63) + command
char serialBuf[SERIAL_BUF_SIZE];
uint8_t serialBufLen = 0;

void handleSerial() {
  while (Serial.available() > 0) {
    char c = Serial.read();

    // obsługa backspace
    if (c == 8 || c == 127) { // 8 = Ctrl+H, 127 = DEL
      if (serialBufLen > 0) {
        serialBufLen--;
        Serial.print("\b \b"); // usuń znak z terminala
      }
      continue;
    }

    // ignoruj inne kontrolne znaki
    if (c < 32 && c != '\n' && c != '\r')
      continue;
    // Enter kończy linię
    if (c == '\n' || c == '\r') {
      serialBuf[serialBufLen] = 0; // terminate string
      if (serialBufLen > 0)
        processSerialCommand(serialBuf);
      serialBufLen = 0; // reset bufora
      memset(serialBuf, 0, sizeof(serialBuf)); // do not keep typed passwords in RAM
      Serial.println(); // nowa linia
      continue;
    }

    // dodaj znak do bufora jeśli jest miejsce
    if (serialBufLen < SERIAL_BUF_SIZE - 1) {
      // do not echo the WiFi password while it is being typed
      bool mask = (serialBufLen >= 9 && strncmp(serialBuf, "wifipass ", 9) == 0) || (serialBufLen >= 8 && strncmp(serialBuf, "webpass ", 8) == 0);
      serialBuf[serialBufLen++] = c;
      Serial.print(mask ? '*' : c); // echo
    }
  }
}

// parser prostych komend
void processSerialCommand(const char *line) {
  // np. rozdziel pierwsze słowo i argument
  char cmd[SERIAL_BUF_SIZE];
  int arg = 0;
  int scanned = sscanf(line, "%s %d", cmd, &arg);
  Serial.println();

  // everything after the command word (used by ssid / wifipass, may contain spaces)
  const char *rest = line;
  while (*rest == ' ')
    rest++;
  rest += strlen(cmd);
  while (*rest == ' ')
    rest++;

  if (strcmp(cmd, "r") == 0) {
    Serial.printf("Registering forced sensor update, interval=%d\r\n", arg);
    registerForceUpdate(arg); // interval w sekundach
  } else if (strcmp(cmd, "i") == 0) {
    Serial.printf("Registering forced import update, interval=%d\r\n", arg);
    registerForceImportUpdate(arg); // interval w sekundach
  } else if (strcmp(cmd, "s") == 0) {
    Serial.println(F("Sensors status:"));
    for (int i = 0; i < sensorsCount; i++) {
      const SensorConfig &s = sensorsSettings[i];
      if (!s.present) {
        continue;
      }
      Serial.printf("%s [%s]: %.2f%s\r\n", s.address, s.name, s.lastValue, getSensorUnits(s).c_str());
    }
  } else if (strcmp(cmd, "mpd") == 0) {
    publishAllHADiscovery();
  } else if (strcmp(cmd, "mps") == 0) {
    mqttPublishChangedSensors(true);
  } else if (strcmp(cmd, "m") == 0) {
    Serial.print(F("MQTT connected: "));
    Serial.println(mqttClient.connected() ? "YES" : "NO");
  } else if (strcmp(cmd, "e") == 0) {
    Serial.println(F("Enviromental data:"));
    Serial.printf("Temperature: %.2fC (upd: %lu)\r\n", globalEnv.temperature, globalEnv.lastTempUpdate);
    Serial.printf("Humidity: %.2f%% (upd: %lu)\r\n", globalEnv.humidity, globalEnv.lastHumiUpdate);
  } else if (strcmp(cmd, "ip") == 0) {
    showWiFiStatus();
  } else if (strcmp(cmd, "ssid") == 0) {
    if (*rest == 0) {
      Serial.printf("Configured SSID: %s\r\n", wifiSSID.length() ? wifiSSID.c_str() : "(none)");
    } else if (strlen(rest) > 32) {
      Serial.println(F("SSID too long (max 32 characters)."));
    } else {
      wifiSSID = rest;
      saveWiFiConfig();
      Serial.println(F("SSID saved. Set the password with 'wifipass', then connect with 'wifi'."));
    }
  } else if (strcmp(cmd, "wifipass") == 0) {
    if (*rest == 0) {
      Serial.println(F("Usage: wifipass <password>  (8-63 characters, or '-' for an open network)"));
    } else if (strcmp(rest, "-") == 0) {
      wifiPassword = "";
      saveWiFiConfig();
      Serial.println(F("WiFi password cleared (open network). Connect with 'wifi'."));
    } else if (strlen(rest) < 8 || strlen(rest) > 63) {
      Serial.println(F("Password must have 8-63 characters (use '-' for an open network)."));
    } else {
      wifiPassword = rest;
      saveWiFiConfig();
      Serial.println(F("WiFi password saved. Connect with 'wifi'."));
    }
  } else if (strcmp(cmd, "webpass") == 0) {
    if (*rest == 0) {
      Serial.println(F("Usage: webpass <password>  (6-63 characters, not 'admin')"));
    } else if (strlen(rest) < 6 || strlen(rest) > 63 || strcmp(rest, "admin") == 0) {
      Serial.println(F("Password must have 6-63 characters and differ from the default."));
    } else {
      webPass = rest;
      saveWiFiConfig();
      Serial.println(F("WWW password saved."));
    }
  } else if (strcmp(cmd, "reboot") == 0) {
    Serial.println(F("Restarting..."));
    delay(100);
    ESP.restart();
  } else if (strcmp(cmd, "wifi") == 0) {
    Serial.println(F("Reconnecting WiFi with saved settings (up to 15 s)..."));
    reconnectWiFi();
  } else if (strcmp(cmd, "help") == 0) {
    Serial.println(F("Available commands:"));
    Serial.println(F("  r [interval] - register forced sensor update with given interval in seconds"));
    Serial.println(F("  i [interval] - register forced import update with given interval in seconds"));
    Serial.println(F("  e - show enviromental data"));
    Serial.println(F("  s - show sensors status"));
    Serial.println(F("  mpd - publish MQTT discovery for all sensors"));
    Serial.println(F("  mps - publish MQTT values for all sensors"));
    Serial.println(F("  m - show MQTT connection status"));
    Serial.println(F("  ip - show WiFi mode, SSID and IP address"));
    Serial.println(F("  ssid [name] - show / set (and save) the WiFi SSID"));
    Serial.println(F("  wifipass <password> - set (and save) the WiFi password; '-' = open network"));
    Serial.println(F("  wifi - reconnect using the saved SSID and password"));
    Serial.println(F("  webpass <password> - set (and save) the WWW password"));
    Serial.println(F("  reboot - restart the device"));
    Serial.println(F("  help - show this help message"));
  } else {
    Serial.printf("Unknown command: %s\r\n", line);
  }
}