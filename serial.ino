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

#define SERIAL_BUF_SIZE 64
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
      Serial.println(); // nowa linia
      continue;
    }

    // dodaj znak do bufora jeśli jest miejsce
    if (serialBufLen < SERIAL_BUF_SIZE - 1) {
      serialBuf[serialBufLen++] = c;
      Serial.print(c); // echo
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

  if (strcmp(cmd, "r") == 0) {
    Serial.printf("Registering forced sensor update, interval=%d\r\n", arg);
    registerForceUpdate(arg); // interval w sekundach
  } else if (strcmp(cmd, "i") == 0) {
    Serial.printf("Registering forced import update, interval=%d\r\n", arg);
    registerForceImportUpdate(arg); // interval w sekundach
  } else if (strcmp(cmd, "s") == 0) {
    Serial.println(F("Sensors status:"));
    for (int i = 0; i < MAX_SENSORS; i++) {
      SensorConfig s = sensorsSettings[i];
      if (!s.present) {
        continue;
      }
      Serial.printf("%s [%s]: %.2f%s\r\n", s.address, s.name, s.lastValue, getSensorUnits(s));
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
    Serial.printf("Temperature: %.2fC (upd: %d)\r\n", globalEnv.temperature, globalEnv.lastTempUpdate);
    Serial.printf("Humidity: %.2f%% (upd: %d)\r\n", globalEnv.humidity, globalEnv.lastHumiUpdate);
  } else if (strcmp(cmd, "help") == 0) {
    Serial.println(F("Available commands:"));
    Serial.println(F("  r [interval] - register forced sensor update with given interval in seconds"));
    Serial.println(F("  i [interval] - register forced import update with given interval in seconds"));
    Serial.println(F("  e - show enviromental data"));
    Serial.println(F("  s - show sensors status"));
    Serial.println(F("  mpd - publish MQTT discovery for all sensors"));
    Serial.println(F("  mps - publish MQTT values for all sensors"));
    Serial.println(F("  m - show MQTT connection status"));
    Serial.println(F("  help - show this help message"));
  } else {
    Serial.printf("Unknown command: %s\r\n", line);
  }
}