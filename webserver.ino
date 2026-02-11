/*
   WWW server
*/

#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>

ESP8266WebServer server(80);
#include <uri/UriRegex.h>

#include "sensors_types.h"

void initWebserver() {
  server.close(); // safe to call even if the server isn't running

  if (WiFi.getMode() == WIFI_AP) {

    server.on("/wifi", HTTP_GET, handleWiFiPage);
    server.on("/wifi", HTTP_POST, handleWiFiPage);

    // files to download
    server.on(UriRegex("^\\/"
                       "(kbSensors\\.css|kbSensors\\.js|kbSensors\\.svg|config\\.json|mqtt\\.json|wifi\\.json)$"),
        []() { handleFileDownload(server.pathArg(0)); });

    // simple captive portal: redirect all requests to /wifi
    server.onNotFound([]() {
      server.sendHeader("Location", "/wifi");
      server.send(302, "text/plain", "");
    });

    server.begin();
    Serial.println("Webserver started on AP mode");

  } else {
    // STA mode!

    server.on("/", []() { handleClientAskingAboutSensors("html"); });

    server.on(UriRegex("^\\/(abc|txt|text|html|xml|json)$"), []() { handleClientAskingAboutSensors(server.pathArg(0)); });

    // files to download
    server.on(UriRegex("^\\/"
                       "(kbSensors\\.css|kbSensors\\.js|kbSensors\\.svg|config\\.json|mqtt\\.json|wifi\\.json)$"),
        []() { handleFileDownload(server.pathArg(0)); });

    // special pages
    server.on("/edit", handleEdit);
    server.on("/reboot", handleReboot);
    server.on("/rescan", handleRescan);
    server.on("/wifi", HTTP_GET, handleWiFiPage);
    server.on("/wifi", HTTP_POST, handleWiFiPage);
    server.on("/mqtt", HTTP_GET, handleMQTTPage);
    server.on("/mqtt", HTTP_POST, handleMQTTPage);

    server.on("/description.xml", HTTP_GET, []() { server.send(200, "text/xml", getSSDPDescriptionString()); });

    server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  };

  server.begin();
  Serial.println("HTTP server started!");
}

#ifdef REMOTE_HELPER_SERVER
// use css, js and svg files from remote server on REMOTE_HELPER_SERVER (eg.
// http://192.168.50.3/kbSensors/),

void handleFileDownload(String file) {
  server.sendHeader("Location", String(REMOTE_HELPER_SERVER) + file, true);
  server.send(302, "text/plain", "");
}

#else
// use local css, js and svg files

void initFileSystem() {
  fileSystemConfig.setAutoFormat(false);
  fileSystem->setConfig(fileSystemConfig);
  fsOK = fileSystem->begin();
  Serial.println(fsOK ? F("Filesystem initialized.") : F("Filesystem init failed!"));
}

bool handleFileDownload(String path) {
  Serial.println(String("handleFileDownload: ") + path);
  if (!fsOK) {
    // replyServerError(FPSTR(FS_INIT_ERROR));
    return true;
  }

  if (path.endsWith("/")) {
    path += "index.htm";
  }

  String contentType;
  if (server.hasArg("download")) {
    contentType = F("application/octet-stream");
  } else {
    contentType = mime::getContentType(path);
  }

  if (!fileSystem->exists(path)) {
    // File not found, try gzip version
    path = path + ".gz";
  }
  if (fileSystem->exists(path)) {
    File file = fileSystem->open(path, "r");
    if (server.streamFile(file, contentType) != file.size()) {
      Serial.println("Sent less data than expected!");
    }
    file.close();
    return true;
  }

  return false;
}
#endif

void handleClientAskingAboutSensors(String desiredFormat) {
  Serial.println("  Client requested " + desiredFormat);
  if ((desiredFormat == "txt") || (desiredFormat == "text")) {
    server.send(200, "text/plain", sendTXT());
    registerForceUpdate(1);
  } else if (desiredFormat == "xml") {
    server.send(200, "text/xml", sendXML());
    registerForceUpdate(1);
  } else if (desiredFormat == "json") {
    server.send(200, "application/json", sendJSON());
    registerForceUpdate(1);
  } else {
    uint16_t refreshPageDuration = 0;
    for (uint8_t i = 0; i < server.args(); i++) {
      if (server.argName(i) == "refresh") {
        // TODO: validate input
        refreshPageDuration = atoi(server.arg(i).c_str());
      }
    }
    server.send(200, "text/html", sendHTML(refreshPageDuration));
    registerForceUpdate(2);
  }
  blink();
}

void handleReboot() {
  Serial.println("Reboot request!");
  server.send(200,
      "text/html",
      F("<html><head><meta http-equiv=\"refresh\" "
        "content=\"10;url=/\"></head><body>Reboot in "
        "progress...</body></html>"));
  blink(3);
  delay(2000); // 2s to allow user to download the page
  ESP.restart();
}

void handleRescan() {
  Serial.println("Rescan request!");
  server.send(200,
      "text/html",
      F("<html><head><meta http-equiv=\"refresh\" "
        "content=\"2;url=/\"></head><body>Rescan in "
        "progress...</body></html>"));
  registerForceUpdate(1);
  blink(4);
}

void handleEdit() {
  String sensorAddressStr = "";
  String sensorNewFriendlyNameStr = "";
  float sensorNewCompensation = 0;

  // get arguments from URL
  for (uint8_t i = 0; i < server.args(); i++) {
    if (server.argName(i) == "address") {
      sensorAddressStr = server.arg(i);
    } else if (server.argName(i) == "friendlyName") {
      sensorNewFriendlyNameStr = server.arg(i);
    } else if (server.argName(i) == "compensation") {
      sensorNewCompensation = atof(server.arg(i).c_str());
    }
  }

  // basic validation
  if ((sensorAddressStr == "") || (sensorNewFriendlyNameStr == "")) {
    Serial.println("Incorrect arguments.");
    server.send(403,
        "text/plain",
        "Arguments error. Use "
        "?address=123456789ABCDEF0&friendlyName=new_name&compensation=1.5");
    return;
  }

  // Address validation (DHTtemp, DHThumi or 16 chars HEX)
  bool isDHT = (sensorAddressStr == "DHTtemp" || sensorAddressStr == "DHThumi");
  if (!isDHT) {
    sensorAddressStr.toUpperCase(); // HEX wielkimi literami
    if (sensorAddressStr.length() != 16) {
      server.send(403, "text/plain", "Address must be 16 hexadecimal characters.");
      Serial.println("Invalid address length.");
      return;
    }
    for (char c : sensorAddressStr) {
      if (!isxdigit(c)) {
        server.send(403, "text/plain", "Address must contain only HEX characters (0-9, A-F).");
        Serial.println("Invalid address characters.");
        return;
      }
    }
  }

  // FriendlyName validation (max 32 chars, only safe characters)
  if (sensorNewFriendlyNameStr.length() > 32) {
    server.send(403, "text/plain", "Friendly name too long (max 32 characters).");
    Serial.println("Friendly name too long.");
    return;
  }
  for (char c : sensorNewFriendlyNameStr) {
    if (c < 32 || c == '<' || c == '>' || c == '&' || c == '"' || c == '\'') {
      server.send(403, "text/plain", "Friendly name contains invalid characters.");
      Serial.println("Friendly name has unsafe characters.");
      return;
    }
  }

  // conversion String -> char[]
  char sensorAddress[17];         // 16 znaków HEX + '\0'
  char sensorNewFriendlyName[33]; // 32 znaki nazwy + '\0'
  sensorAddressStr.toCharArray(sensorAddress, sizeof(sensorAddress));
  sensorNewFriendlyNameStr.toCharArray(sensorNewFriendlyName, sizeof(sensorNewFriendlyName));

  // Debug
  Serial.printf("New name of sensor %s is %s; compensation = %.2f", sensorAddress, sensorNewFriendlyName, sensorNewCompensation);
  Serial.println();

  if (editSensor(sensorAddress, sensorNewFriendlyName, sensorNewCompensation)) {
    saveConfig();
    server.send(200, "text/plain", "Sensor saved!");
    Serial.println("Sensor saved.");
  } else {
    server.send(403, "text/plain", "Error while saving sensor.");
    Serial.println("Error while saving sensor (Sensor not found & no space for more).");
  }
}

// Returns true on success, false if sensor not found and no space for more
bool editSensor(const char *sensorAddress, const char *newFriendlyName, float newCompensation) {
  for (uint16_t i = 0; i < sensorsCount; i++) {
    if (strncmp(sensorsSettings[i].address, sensorAddress, sizeof(sensorsSettings[i].address)) == 0) {
      // Aktualizacja istniejącego sensora
      strlcpy(sensorsSettings[i].name, newFriendlyName, sizeof(sensorsSettings[i].name));
      sensorsSettings[i].compensation = newCompensation;
      return true;
    }
  }

  // If not found, add new sensor if there is space
  if (sensorsCount < MAX_SENSORS) {
    strlcpy(sensorsSettings[sensorsCount].address, sensorAddress, sizeof(sensorsSettings[sensorsCount].address));
    strlcpy(sensorsSettings[sensorsCount].name, newFriendlyName, sizeof(sensorsSettings[sensorsCount].name));
    sensorsSettings[sensorsCount].compensation = newCompensation;
    sensorsSettings[sensorsCount].present = false; // newly added sensor = not checked as present yet
    sensorsCount++;
    return true;
  }

  // Brak miejsca
  return false;
}

String sendHTML(uint16_t refreshPageDuration) {
  String valueString;
  float value;

  String ptr = "<!DOCTYPE html>\n";
  ptr += "<html>\n";
  ptr += "<head>\n";
  ptr += "  <meta name=\"viewport\" content=\"width=device-width, "
         "initial-scale=1.0\">\n";
  ptr += "  <meta charset=\"UTF-8\">\n";
  ptr += "  <title>" + (String)programName + "</title>\n";
  ptr += "  <link rel=\"stylesheet\" href=\"/kbSensors.css\">\n";
  ptr += "  <script src=\"/kbSensors.js\" defer></script>\n";
  ptr += "  <link rel=\"icon\" href=\"/kbSensors.svg\" type=\"image/svg+xml\">\n";
  ptr += "  <meta name=\"generator\" content=\"" + (String)programName + " " + (String)programVersion + "\">\n";
  ptr += "  <meta name=\"description\" content=\"" + (String)programManual + "\">\n";

  if (refreshPageDuration > 0) {
    ptr += "  <meta http-equiv=\"refresh\" content=\"" + (String)refreshPageDuration + "\">\n";
  }

  ptr += "</head>\n";
  ptr += "<body><form><div id=\"webpage\">\n";
  ptr += "<h1>" + (String)programName + "</h1>\n";

  if (sensorsCount > 0) {
    ptr += "<table>\n";
    for (uint16_t i = 0; i < sensorsCount; i++) {
      SensorConfig s = sensorsSettings[i];
      if (!s.present) {
        continue;
      }

      ptr += "<tr>\n";
      ptr += "  <td title=" + (String)s.address + ">" + (String)s.name + "\n";
      value = s.lastValue + s.compensation;
      if (value < 0) {
        valueString = (String) "&minus;" + (String)(-value);
      } else {
        valueString = (String)value;
      }
      ptr += "  <td title=\"" + (String)(s.compensation) + "\">" + valueString + (String)getSensorUnits(s) + "\n";
      ptr += "</tr>\n";
    }

    /* nav */
    ptr += "<tr>\n";
    ptr += "  <td colspan=\"2\" id=\"navtr\">\n";
    ptr += "    <div id=\"settingsContainer\">\n";
    ptr += "      <span id=\"settingsIcon\">⚙️</span>\n";
    ptr += "      <nav id=\"settingsMenu\">\n";
    ptr += "        <a href=\"#\" onclick=\"refreshPage(event);\">refresh "
           "page</a> | \n";
    ptr += "        <a href=\"reboot\">reboot kbSensors</a> | \n";
    ptr += "        <a href=\"rescan\">rescan sensors</a> | \n";
    ptr += "        <a href=\"wifi\">settings</a> | \n";
    ptr += "        <a href=\"mqtt\">mqtt</a>\n";
    ptr += "      </nav>\n";
    ptr += "    </div>\n";
    ptr += "  </td>\n";
    ptr += "</tr>\n";

    ptr += "</table>\n";
  }

  ptr += "</div></form></body>\n";
  ptr += "</html>";
  return ptr;
}

String sendXML() {
  String ptr = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  ptr += "<report>\n";

  for (uint16_t i = 0; i < sensorsCount; i++) {
    SensorConfig s = sensorsSettings[i];
    if (!s.present) {
      continue;
    }

    ptr += "<sensor address=\"" + String(s.address) + "\">\n";
    ptr += "  <name>" + String(s.name) + "</name>\n";
    ptr += "  <value>" + String(s.lastValue + s.compensation) + "</value>\n";
    ptr += "  <type>" + String(sensorTypeStrs[s.type]) + "</type>\n";
    ptr += "</sensor>\n";
  }
  ptr += "</report>";
  return ptr;
}

String makeSafeName(const char *unsafeName) {
  String safe = unsafeName; // konwersja char[] -> String
  safe.replace(" ", "_");   // zamiana spacji na _
  return safe;
}

String sendTXT() {
  String output = "";

  for (uint16_t i = 0; i < sensorsCount; i++) {
    SensorConfig s = sensorsSettings[i];
    if (!s.present) {
      continue;
    }

    float adjustedValue = s.lastValue + s.compensation;
    output += makeSafeName(s.name) + ".value " + String(adjustedValue, 2) + "\n";
  }

  return output;
}

String sendJSON() {
  StaticJsonDocument<1024> doc; // większy, bo może być dużo DS18B20
  JsonArray dsArray = doc.createNestedArray("sensors");

  for (uint16_t i = 0; i < sensorsCount; i++) {
    SensorConfig s = sensorsSettings[i];
    if (!s.present) {
      continue;
    }
    JsonObject o = dsArray.createNestedObject();
    o["name"] = s.name;
    o["address"] = s.address;
    o["value"] = s.lastValue;
    o["compensation"] = s.compensation;
    o["type"] = getSensorType(s);
  }

  String response;
  serializeJson(doc, response);
  return response;
}

void handleWiFiPage() {
  String message;
  // sprawdzenie autoryzacji basic auth
  if (!server.authenticate(webUser.c_str(), webPass.c_str())) {
    server.requestAuthentication();
    return;
  }

  if (server.method() == HTTP_POST) {
    if (server.hasArg("ssid"))
      wifiSSID = server.arg("ssid");
    if (server.hasArg("wifipassword"))
      wifiPassword = server.arg("wifipassword");
    if (server.hasArg("webuser"))
      webUser = server.arg("webuser");

    if (server.hasArg("webpass") && server.hasArg("webpass2")) {
      String pass1 = server.arg("webpass");
      String pass2 = server.arg("webpass2");

      if (pass1.isEmpty() || pass2.isEmpty()) {
        message = F("<p style='color:red'>WWW password cannot be empty!</p>");
      } else if (pass1 != pass2) {
        message = F("<p style='color:red'>WWW passwords do not match!</p>");
      } else {
        webPass = pass1;
        message = F("<html><head><meta http-equiv=\"refresh\" "
                    "content=\"10;url=/\"></head><body>Settings saved. Reboot "
                    "in progress...</body></html>");
      }
    }

    saveWiFiConfig();
    server.send(200, "text/html", message);
    blink(3);
    delay(2000); // 2s to allow user to download the page
    ESP.restart();

    return;
  }

  // Build HTML page
  String html = "<html><head>";
  html += "<link rel='stylesheet' href='/kbSensors.css'>";
  html += "<script src='/kbSensors.js'></script>";
  html += "<title>WiFi Configuration</title></head><body>";
  html += "<h2>WiFi Configuration</h2>";
  html += message;
  html += "<form method='POST' onsubmit='return validateWebPasswordForm();'>";
  html += "SSID: <input name='ssid' value='" + wifiSSID + "'><br>";
  html += "WiFi Password: <input name='wifipassword' value='" + wifiPassword + "' type='password'><hr>";
  html += "Username: <input name='webuser' value='" + webUser + "'><br>";
  html += "WWW Password: <input name='webpass' value='" + webPass + "' type='password'><br>";
  html += "Confirm WWW Password: <input name='webpass2' value='" + webPass + "' type='password'><hr>";
  html += "<input type='submit' value='Save'>";
  html += "</form></body></html>";

  server.send(200, "text/html", html);
}

void handleMQTTPage() {
  String message;

  // Basic auth jak w WiFi
  if (!server.authenticate(webUser.c_str(), webPass.c_str())) {
    server.requestAuthentication();
    return;
  }

  if (server.method() == HTTP_POST) {

    // DEBUG: Wypisz co przyszło
    Serial.println("POST Arguments received:");
    for (int i = 0; i < server.args(); i++) {
      Serial.printf("Arg '%s' = '%s'\n", server.argName(i).c_str(), server.arg(i).c_str());
    }

    if (server.hasArg("host"))
      mqtt.host = server.arg("host");
    if (server.hasArg("port"))
      mqtt.port = server.arg("port").toInt();
    if (server.hasArg("user"))
      mqtt.user = server.arg("user");
    if (server.hasArg("pass"))
      mqtt.pass = server.arg("pass");
    if (server.hasArg("baseTopic"))
      mqtt.baseTopic = server.arg("baseTopic");
    if (server.hasArg("enabled"))
      mqtt.enabled = server.arg("enabled") == "on";
    if (server.hasArg("qos"))
      mqtt.qos = server.arg("qos").toInt();
    if (server.hasArg("retain"))
      mqtt.retain = server.arg("retain") == "on";

    if (server.hasArg("remTempMethod"))
      config.compTemp.method = (ImportMethod)server.arg("remTempMethod").toInt();
    if (server.hasArg("remTempSource"))
      config.compTemp.source = server.arg("remTempSource");
    if (server.hasArg("remTempParam")) {
      strlcpy(config.compTemp.address, server.arg("remTempParam").c_str(), sizeof(config.compTemp.address));
    }

    if (server.hasArg("remHumiMethod"))
      config.compHumi.method = (ImportMethod)server.arg("remHumiMethod").toInt();
    if (server.hasArg("remHumiSource"))
      config.compHumi.source = server.arg("remHumiSource");
    if (server.hasArg("remHumiParam")) {
      strlcpy(config.compHumi.address, server.arg("remHumiParam").c_str(), sizeof(config.compHumi.address));
    }

    if (server.hasArg("reset")) {
      mqtt = MQTTConfig(); // przywróć domyślne wartości
      message = "<p>MQTT settings reset to defaults</p>";
    } else {
      message = "<p>MQTT settings saved</p>";
    }

    saveConfig();
    saveMQTTConfig();
    server.send(200,
        "text/html",
        F("<html><head><meta http-equiv=\"refresh\" "
          "content=\"2;url=/\"></head><body>") +
            message + F("</body></html>"));
    blink(3);
    return;
  }

  // Formularz HTML
  String html = "<html><head>";
  html += "<link rel='stylesheet' href='/kbSensors.css'>";
  html += "<script src='/kbSensors.js'></script>";
  html += "<title>MQTT Configuration</title></head><body>";
  html += "<h2>MQTT Configuration</h2>";
  html += message;
  html += "<form method='POST'>";
  html += "Host: <input name='host' value='" + mqtt.host + "'><br>";
  html += "Port: <input name='port' value='" + String(mqtt.port) + "'><br>";
  html += "User: <input name='user' value='" + mqtt.user + "'><br>";
  html += "Password: <input name='pass' value='" + mqtt.pass + "' type='password'><br>";
  html += "Base Topic: <input name='baseTopic' value='" + mqtt.baseTopic + "'><br>";
  html += "Enabled: <input type='checkbox' name='enabled'" + String(mqtt.enabled ? " checked" : "") + "><br>";
  html += "QoS: <input name='qos' value='" + String(mqtt.qos) + "'><br>";
  html += "Retain: <input type='checkbox' name='retain'" + String(mqtt.retain ? " checked" : "") + "><br><hr>";
  //
  // temporarily here as we should think about reconstructing the settings page.
  html +=
      "<fieldset>"
      "<legend> Kompensacja srodowiskowa (dla CCS811)</legend>"

      "<h4>Temperatura</h4><select id='remTempMethod' name='remTempMethod' onchange='toggleRemoteInputs()'><option value='0'>Brak</option>"
      "<option value='1'>MQTT Topic</option><option value='2'>JSON URL</option></select>"
      "<input type='text' id='remTempSource' name='remTempSource' placeholder='URL lub Temat MQTT'>"
      "<input type='text' id='remTempParam' name='remTempParam' placeholder='Adres sensora (dla JSON)'><br />"

      "<h4>Wilgotnosc</h4><select id='remHumiMethod' name='remHumiMethod' onchange='toggleRemoteInputs()'><option value='0'>Brak</option>"
      "<option value='1'>MQTT Topic</option><option value='2'>JSON URL</option></select>"
      "<input type='text' id='remHumiSource' name='remHumiSource' placeholder='URL lub Temat MQTT'>"
      "<input type='text' id='remHumiParam'  name='remHumiParam' placeholder='Adres sensora (dla JSON)'></fieldset> ";
  html += "<hr>";
  html += "<input type='submit' value='Save'>";
  html += " <input type='submit' name='reset' value='Reset to defaults'>";
  html += "</form></body></html>";

  server.send(200, "text/html", html);
}

void handleWebserver() { server.handleClient(); }
