const char programName[] =
  "KB Sensors station.";

const char programVersion[] =
  "0.20251225tmp";

const char programManual[] =
  "// tiny monitor station for one DHT11 and many DS18B20 sensors\n"
  "// outputs data at http://kbsensors/ , http://kbsensors/xml , http://kbsensors/txt\n"
  "// https is not available for now. Optional ?refresh=[seconds] for html output.";



// copy wifiConfig_sample.h to wifiConfig.h and enter your ssid/password there
#include "wifiConfig.h"
// (TODO: config using www?? naah.)


/* uncomment the following line to use REMOTE_HELPER_SERVER instead of local files for:
  kbSensors.js, kbSensors.css and kbSensors.svg
*/
// #define REMOTE_HELPER_SERVER "http://192.168.50.3/kbSensors/"

// comment the following line to disable OTA updates
#define OTA_ENABLED

/*
   WWW server
*/
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
ESP8266WebServer server(80);
#include <uri/UriRegex.h>
#include <ArduinoJson.h>

/*
   filesystem
*/
#include <LittleFS.h>
const char* fsName = "LittleFS";
FS* fileSystem = &LittleFS;
LittleFSConfig fileSystemConfig = LittleFSConfig();
static bool fsOK;

/*
   friendly "kbsensors" name
*/
#define NETBIOSNAME "kbsensors"
#include <ESP8266NetBIOS.h>

/*
   updates over the air
*/
#ifdef OTA_ENABLED
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#endif


/*************************************
   SENSORS:
*/

// the sensors are slow; you can read them not more often than 1s.
const unsigned int updateInterval = 1000;
unsigned long previousMillis = 0;

/*
   DHT11
*/
#include "DHTStable.h"
#define DHT11_PIN 4  // connect DHT11 to the D2 pin on NodeMCU v3 (don't forget about the resistor)
DHTStable DHT;

/*
   DS18B20
*/
#include <OneWire.h>
#include <DallasTemperature.h>
#define DS18B20_PIN 5  // Connect DS18B20 to D1 pin on NodeMCU v3 (don't forget about the resistor)
OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);
DeviceAddress Thermometer;

/*
   tables: results
*/
// we write these tables when reading sensors
// and read these tables when sending results to user
uint8_t sensorResultCount = 0;
#define MAX_SENSORS 250
#define MAXRESULTSCOUNT 250

String sensorAddresses[MAXRESULTSCOUNT];  // String, because we use also "DHTtemp" and "DHThumi"; that's also good for easy adding future sensors
String friendlyNames[MAXRESULTSCOUNT];
float values[MAXRESULTSCOUNT];
float compensation[MAXRESULTSCOUNT];
String valueType[MAXRESULTSCOUNT];

// for valueType[]
const String TYPE_PERCENT = "%";    // correct notation for Polish language (no space before percent sign)
const String TYPE_CDEGREE = " °C";  // correct notation for Polish language (space, degree sign, C)

/*
   tables: sensorsDB
*/
// this is the sensors database, it is (will be) stored on the device (in EEPROM). Compensation and friendly names.
#define CONFIG_FILE "/config.json"
struct SensorConfig {
  char address[17];    // 16 hex + \0
  char name[32];       // friendly name
  float compensation;  // korekta
  bool present;        // runtime only
};
SensorConfig sensorsSettings[MAX_SENSORS];
uint16_t sensorsCount = 0;


/*
   LED for blinking
*/
uint16_t blinkDelay = 100;  // 0.1 s
const uint8_t ledPin = LED_BUILTIN;

void setup() {
  // put your setup code here, to run once:
  initSerial();

#ifndef REMOTE_HELPER_SERVER
  initFileSystem();
#endif

  initWiFi();

#ifdef OTA_ENABLED
  initOTA();
#endif

  initNetbiosName();
  initWebserver();
  loadConfig();
  sensors.begin();  // ds18b20 init
  initLED();
}

void initLED() {
  pinMode(ledPin, OUTPUT);
  blink(2);
}

void initSerial() {
  Serial.begin(74880);
  Serial.setTimeout(50);
  showAbout();
  Serial.println("Serial initialized!");
}

void showAbout() {
  Serial.println("\n\n");
  Serial.println(programName);
  Serial.println(programVersion);
  Serial.println(programManual);
  Serial.println("\n\n");
}

void initNetbiosName() {
  NBNS.begin(NETBIOSNAME);
}

void initOTA() {
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // let's do it consistent with the netbios name
  ArduinoOTA.setHostname(NETBIOSNAME);

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

bool loadConfig() {
  if (!LittleFS.exists(CONFIG_FILE)) {
    Serial.println("Config not found");
    sensorsCount = 0;
    return false;
  }

  File f = LittleFS.open(CONFIG_FILE, "r");
  if (!f) return false;

  DynamicJsonDocument doc(20000);
  auto err = deserializeJson(doc, f);
  f.close();

  if (err) {
    Serial.println("JSON parse error");
    return false;
  }

  JsonArray arr = doc["sensors"];
  if (!arr) return false;

  sensorsCount = 0;

  for (JsonObject o : arr) {
    if (sensorsCount >= MAX_SENSORS) break;

    strlcpy(sensorsSettings[sensorsCount].address,
            o["address"] | "",
            sizeof(sensorsSettings[sensorsCount].address));

    strlcpy(sensorsSettings[sensorsCount].name,
            o["name"] | "Sensor",
            sizeof(sensorsSettings[sensorsCount].name));

    sensorsSettings[sensorsCount].compensation =
      o["compensation"] | 0.0f;

    sensorsSettings[sensorsCount].present = false;
    sensorsCount++;
  }

  Serial.printf("Loaded %u sensors from config\n", sensorsCount);
  return true;
}

/*
void loadSensorsDB() {
  // TODO: load data from EEPROM.
  sensorsDBAddresses[0] = "28D8BE75D0013C7F";
  sensorsDBFriendlyNames[0] = "pokój";
  sensorsDBCompensation[0] = 0;

  sensorsDBAddresses[1] = "2864EB75D0013C42";
  sensorsDBFriendlyNames[1] = "zamrazalnik";
  sensorsDBCompensation[1] = -0.8;

  sensorsDBAddresses[2] = "28A3AC75D0013CE0";
  sensorsDBFriendlyNames[2] = "lodowka";
  sensorsDBCompensation[2] = 0;

  sensorsDBAddresses[3] = "DHTtemp";
  sensorsDBFriendlyNames[3] = "Temperatura DHT";
  sensorsDBCompensation[3] = 0;

  sensorsDBAddresses[4] = "28A1C70175230BE7";
  sensorsDBFriendlyNames[4] = "na dworze";
  sensorsDBCompensation[4] = 0;

  sensorsDBCount = 5;
}
*/

bool saveConfig() {
  DynamicJsonDocument doc(20000);

  doc["version"] = programVersion;
  JsonArray arr = doc.createNestedArray("sensors");

  for (uint16_t i = 0; i < sensorsCount; i++) {
    JsonObject o = arr.createNestedObject();
    o["address"] = sensorsSettings[i].address;
    o["name"] = sensorsSettings[i].name;
    o["compensation"] = sensorsSettings[i].compensation;
  }

  File f = LittleFS.open(CONFIG_FILE, "w");
  if (!f) return false;

  serializeJsonPretty(doc, f);
  f.close();

  Serial.println("Config saved");
  return true;
}

void saveSensorsDB() {
  // TODO
}

void initWiFi() {
  Serial.print("Connecting to ");
  Serial.print(ssid);

  //connect to your local wi-fi network
  WiFi.begin(ssid, password);

  //check wi-fi is connected to wi-fi network
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected..!");
  Serial.print("Got IP: ");
  Serial.println(WiFi.localIP());
}

void initWebserver() {
  server.on("/", []() {
    handleClientAskingAboutSensors("html");
  });
  server.on(UriRegex("^\\/(abc|txt|text|html|xml)$"), []() {
    handleClientAskingAboutSensors(server.pathArg(0));
  });

  // files to download
  server.on(UriRegex("^\\/(kbSensors\\.css|kbSensors\\.js|kbSensors\\.svg|config\\.json)$"), []() {
    handleFileDownload(server.pathArg(0));
  });

  // special pages
  server.on("/edit", handleEdit);
  server.on("/reboot", handleReboot);
  server.on("/reset", handleReset);  // reset only resets sensors.

  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });

  server.begin();
  Serial.println("HTTP server started!");
}

void loop() {
  // put your main code here, to run repeatedly:
  // updateSensorsValues();
  server.handleClient();

#ifdef OTA_ENABLED
  ArduinoOTA.handle();
#endif
}

#ifdef REMOTE_HELPER_SERVER
// use css, js and svg files from remote server on REMOTE_HELPER_SERVER (eg. http://192.168.50.3/kbSensors/),

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
  // bool updated = updateSensorsValues();
  bool updated = scanSensors();
  if ((desiredFormat == "txt") || (desiredFormat == "text")) {
    server.send(200, "text/plain", sendTXT());
  } else if (desiredFormat == "xml") {
    server.send(200, "text/xml", sendXML());
  } else {
    uint16_t refresh = 0;
    for (uint8_t i = 0; i < server.args(); i++) {
      if (server.argName(i) == "refresh") {
        // TODO: validate input
        refresh = atoi(server.arg(i).c_str());
      }
    }
    server.send(200, "text/html", sendHTML(refresh));
    if (updated) {
      blink();
    }
  }
}

void handleReboot() {
  Serial.println("Reboot request!");
  server.send(200, "text/html", F("<html><head><meta http-equiv=\"refresh\" content=\"10;url=/\"></head><body>Reboot in progress...</body></html>"));
  blink(3);
  delay(2000);  // 2s to allow user to download the page
  ESP.restart();
}

void handleReset() {
  Serial.println("Reset request!");
  server.send(200, "text/html", F("<html><head><meta http-equiv=\"refresh\" content=\"2;url=/\"></head><body>Reset in progress...</body></html>"));
  sensors.begin();  // ds18b20 init
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
    server.send(403, "text/plain", "Arguments error. Use ?address=123456789ABCDEF0&friendlyName=new_name&compensation=1.5");
    return;
  }

  // address validation (16 chars HEX)
  sensorAddressStr.toUpperCase();  // HEX wielkimi literami
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
  char sensorAddress[17];          // 16 znaków HEX + '\0'
  char sensorNewFriendlyName[33];  // 32 znaki nazwy + '\0'
  sensorAddressStr.toCharArray(sensorAddress, sizeof(sensorAddress));
  sensorNewFriendlyNameStr.toCharArray(sensorNewFriendlyName, sizeof(sensorNewFriendlyName));

  // Debug
  Serial.printf("New name of sensor %s is %s; compensation = %.2f\n", sensorAddress, sensorNewFriendlyName, sensorNewCompensation);

  // było
  // Serial.println("New name of sensor " + sensorAddressStr + " is " + sensorNewFriendlyNameStr + "; compensation = " + (String)sensorNewCompensation + ".");

  if (editSensor(sensorAddress, sensorNewFriendlyName, sensorNewCompensation)) {
    saveConfig();
    server.send(200, "text/plain", "Sensor saved!");
    Serial.println("Sensor saved.");
  } else {
    server.send(403, "text/plain", "Error while saving sensor.");
    Serial.println("Error while saving sensor (Sensor not found & no space for more).");
  }
}

// Zwraca true, jeśli sensor został znaleziony i edytowany
bool editSensor(const char* sensorAddress, const char* newFriendlyName, float newCompensation) {
  for (uint16_t i = 0; i < sensorsCount; i++) {
    if (strncmp(sensorsSettings[i].address, sensorAddress, sizeof(sensorsSettings[i].address)) == 0) {
      // Aktualizacja istniejącego sensora
      strlcpy(sensorsSettings[i].name, newFriendlyName, sizeof(sensorsSettings[i].name));
      sensorsSettings[i].compensation = newCompensation;
      return true;
    }
  }

  // Jeśli nie znaleziono – dodajemy nowy sensor (jeśli jest miejsce)
  if (sensorsCount < MAX_SENSORS) {
    strlcpy(sensorsSettings[sensorsCount].address, sensorAddress, sizeof(sensorsSettings[sensorsCount].address));
    strlcpy(sensorsSettings[sensorsCount].name, newFriendlyName, sizeof(sensorsSettings[sensorsCount].name));
    sensorsSettings[sensorsCount].compensation = newCompensation;
    sensorsSettings[sensorsCount].present = false;  // nowo dodany sensor jeszcze nie wykryty
    sensorsCount++;
    return true;
  }

  // Brak miejsca
  return false;
}


String sendHTML(uint16_t refresh) {
  String valueString;
  float value;

  String ptr = "<!DOCTYPE html>\n";
  ptr += "<html>\n";
  ptr += "<head>\n";
  ptr += "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
  ptr += "  <meta charset=\"UTF-8\">\n";
  ptr += "  <title>" + (String)programName + "</title>\n";
  ptr += "  <link rel=\"stylesheet\" href=\"/kbSensors.css\">\n";
  ptr += "  <script src=\"/kbSensors.js\" defer></script>\n";
  ptr += "  <link rel=\"icon\" href=\"/kbSensors.svg\" type=\"image/svg+xml\">\n";
  ptr += "  <meta name=\"generator\" content=\"" + (String)programName + " " + (String)programVersion + "\">\n";
  ptr += "  <meta name=\"description\" content=\"" + (String)programManual + "\">\n";

  if (refresh > 0) {
    ptr += "  <meta http-equiv=\"refresh\" content=\"" + (String)refresh + "\">\n";
  }

  ptr += "</head>\n";
  ptr += "<body><form><div id=\"webpage\">\n";
  ptr += "<h1>" + (String)programName + "</h1>\n";

  if (sensorResultCount > 0) {
    ptr += "<table>\n";
    for (uint8_t deviceNumber = 0; deviceNumber < sensorResultCount; deviceNumber++) {
      ptr += "<tr>\n";
      ptr += "  <td title=" + (String)sensorAddresses[deviceNumber] + ">" + (String)friendlyNames[deviceNumber] + "\n";
      value = values[deviceNumber] + compensation[deviceNumber];
      if (value < 0) {
        valueString = (String) "&minus;" + (String)(-value);
      } else {
        valueString = (String)value;
      }
      ptr += "  <td title=\"" + (String)compensation[deviceNumber] + "\">" + valueString + valueType[deviceNumber] + "\n";
      ptr += "</tr>\n";
    }
    /* nav */
    ptr += "\n<tr><td colspan=\"2\" id=\"navtr\"><nav>\n";
    ptr += "  <a href=\"#\" onclick=\"refreshPage(event);\">refresh</a> | \n";
    ptr += "  <a href=\"reboot\">reboot</a> | \n";
    ptr += "  <a href=\"reset\">reset</a>\n";
    ptr += "</nav></td></tr>\n";

    ptr += "</table>\n";
  }

  ptr += "</div></form></body>\n";
  ptr += "</html>";
  return ptr;
}

String sendXML() {
  String ptr = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  ptr += "<report>\n";

  for (uint8_t deviceNumber = 0; deviceNumber < sensorResultCount; deviceNumber++) {
    ptr += "<sensor address=\"" + (String)sensorAddresses[deviceNumber] + "\">\n";
    ptr += "  <name>" + (String)friendlyNames[deviceNumber] + "</name>\n";
    ptr += "  <value>" + (String)(values[deviceNumber] + compensation[deviceNumber]) + "</value>\n";
    ptr += "  <type>" + (String)valueType[deviceNumber] + "</type>\n";
    // ptr += "  <address>" + (String)sensorAddresses[deviceNumber] + "</address>\n";
    ptr += "</sensor>\n";
  }
  ptr += "</report>";
  return ptr;
}

String sendTXT() {
    String output = "";

    for (uint8_t i = 0; i < sensorResultCount; i++) {
        // if (!sensorsSettings[i].present) continue; // tylko wykryte sensory

        // Zamiana spacji w nazwie na "_", żeby nie psuło TXT/XML/CSV
        char safeName[33];
        strlcpy(safeName, sensorsSettings[i].name, sizeof(safeName));
        for (uint8_t j = 0; j < strlen(safeName); j++) {
            if (safeName[j] == ' ') safeName[j] = '_';
        }

        float adjustedValue = values[i] + sensorsSettings[i].compensation;

        output += String(safeName) + ".value " + String(adjustedValue, 2) + "\n";
    }

    return output;
}

/* //OLD 
String sendTXT() {
  String ptr = "";

  for (uint8_t deviceNumber = 0; deviceNumber < sensorResultCount; deviceNumber++) {
    // TODO: replace spaces with "_"
    ptr += (String)friendlyNames[deviceNumber] + ".value " + (String)(values[deviceNumber] + compensation[deviceNumber]) + "\n";
  }
  return ptr;
}*/

/*
bool updateSensorsValues() {
  unsigned long currentMillis = millis();

  if (currentMillis < previousMillis) {
    // millis() returns the number of milliseconds passed since the Arduino board began running the current program.
    // This number will overflow (go back to zero), after approximately 50 days.
    // This just happened.
    previousMillis = -updateInterval;
  }

  if (currentMillis - previousMillis < updateInterval) {
    // too early from previous update
    return false;
  }

  previousMillis = currentMillis;

  // we can reset the ResultCount.
  sensorResultCount = 0;
  updateDHTValues();
  updateDS18B20Values();
  emptyRestOfTheArray();
  return true;
}


boolean addSensor(String address, float value, String sensorType) {
  if ((sensorResultCount > MAXRESULTSCOUNT) || isnan(value)) {
    return false;
  }

  values[sensorResultCount] = value;
  valueType[sensorResultCount] = sensorType;
  sensorAddresses[sensorResultCount] = address;

  // find sensor address in database
  uint8_t sensorIndexInDB = getSensorsDBIndex(address);
  if (sensorIndexInDB != 255) {
    friendlyNames[sensorResultCount] = sensorsDBFriendlyNames[sensorIndexInDB];
    compensation[sensorResultCount] = sensorsDBCompensation[sensorIndexInDB];
  } else {
    friendlyNames[sensorResultCount] = address;
  }
  sensorResultCount++;
  return true;
}

void emptyRestOfTheArray() {
  for (uint8_t sensorIndexInResults = sensorResultCount; sensorIndexInResults < MAXRESULTSCOUNT; sensorIndexInResults++) {
    sensorAddresses[sensorIndexInResults] = "";
    values[sensorIndexInResults] = 0;
    friendlyNames[sensorIndexInResults] = "";
    valueType[sensorIndexInResults] = "";
  }
}

void updateDHTValues() {
  uint8_t sensorIndexInDB = 255;
  float temperature, humidity;

  int chk = DHT.read11(DHT11_PIN);
  if (chk == DHTLIB_OK) {
    temperature = DHT.getTemperature();
    if (!isnan(temperature)) {
      addSensor("DHTtemp", temperature, TYPE_CDEGREE);
      Serial.println("DHT temperature: " + (String)temperature + "°C");
    } else {
      Serial.println("Failed to read temperature from DHT sensor!");
    }

    humidity = DHT.getHumidity();
    if (!isnan(humidity)) {
      addSensor("DHThumi", humidity, TYPE_PERCENT);
      Serial.println("DHT humidity: " + (String)humidity + "%.");
    } else {
      Serial.println("Failed to read humidity from DHT sensor!");
    }
  }
}
*/

float getTemperature(uint16_t idx) {
  if (!sensorsSettings[idx].present) return NAN;

  DeviceAddress addr;
  hexStringToAddress(sensorsSettings[idx].address, addr);

  float t = sensors.getTempC(addr);
  if (t == DEVICE_DISCONNECTED_C) return NAN;

  return t + sensorsSettings[idx].compensation;
}

/*
void updateDS18B20Values() {
  float temperature;
  String address;

  uint8_t deviceCount = sensors.getDeviceCount();
  Serial.println((String)deviceCount + " sensors found on OneWire.");

  sensors.requestTemperatures();

  for (uint8_t deviceNumber = 0; deviceNumber < deviceCount; deviceNumber++) {
    temperature = sensors.getTempCByIndex(deviceNumber);
    sensors.getAddress(Thermometer, deviceNumber);
    address = formatAddressAsHex(Thermometer);

    if (temperature > -127) {  // valid result
      addSensor(address, temperature, TYPE_CDEGREE);
      Serial.println("sensor_" + address + "=" + (String)temperature);
    } else {
      Serial.println("Skipping sensor_" + address + " because its reading is " + (String)temperature + ".");
    }
  }
}

uint8_t getSensorsDBIndex(String deviceAddress) {
  for (uint8_t index = 0; index < sensorsDBCount; index++) {
    if (sensorsDBAddresses[index] == deviceAddress) {
      return index;
    }
  }
  return 255;
}

*/


bool scanSensors() {
  DeviceAddress addr;
  bool configChanged = false;

  // reset present flags
  for (uint16_t i = 0; i < sensorsCount; i++) {
    sensorsSettings[i].present = false;
  }

  uint8_t found = sensors.getDeviceCount();

  for (uint8_t i = 0; i < found; i++) {
    if (!sensors.getAddress(addr, i)) continue;

    char addrStr[17];
    for (uint8_t j = 0; j < 8; j++) {
      sprintf(&addrStr[j * 2], "%02X", addr[j]);
    }
    addrStr[16] = '\0';

    // czy już istnieje?
    int16_t idx = findSensorByAddress(addrStr);

    if (idx >= 0) {
      sensorsSettings[idx].present = true;
    } else if (sensorsCount < MAX_SENSORS) {
      // nowy sensor
      strlcpy(sensorsSettings[sensorsCount].address, addrStr, 17);
      snprintf(sensorsSettings[sensorsCount].name,
               sizeof(sensorsSettings[sensorsCount].name),
               "Sensor %u", sensorsCount + 1);
      sensorsSettings[sensorsCount].compensation = 0.0f;
      sensorsSettings[sensorsCount].present = true;
      sensorsCount++;
      configChanged = true;
    } else {
      Serial.println("Warning: skipping sensor (MAX_SENSORS limit).");
    }
  }

  if (configChanged) {
    saveConfig();
  }

  return true;
}

String formatAddressAsHex(DeviceAddress deviceAddress) {
  String address = "";
  uint8_t oneByte;
  char hexChars[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
  for (uint8_t i = 0; i < 8; i++) {
    oneByte = deviceAddress[i];
    address += hexChars[(oneByte & 0xF0) >> 4];
    address += hexChars[oneByte & 0x0F];
  }
  return address;
}

int16_t findSensorByAddress(const char* addr) {
  for (uint16_t i = 0; i < sensorsCount; i++) {
    if (strcmp(sensorsSettings[i].address, addr) == 0)
      return i;
  }
  return -1;
}

// Zamienia 16-znakowy string HEX na tablicę 8 bajtów
bool hexStringToAddress(const char* hexString, uint8_t* addr) {
    if (strlen(hexString) != 16) return false; // walidacja długości

    for (uint8_t i = 0; i < 8; i++) {
        char high = hexString[i * 2];
        char low  = hexString[i * 2 + 1];

        auto hexToNibble = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            return -1; // invalid char
        };

        int h = hexToNibble(high);
        int l = hexToNibble(low);

        if (h == -1 || l == -1) return false; // niepoprawny znak HEX

        addr[i] = (h << 4) | l;
    }
    return true;
}


void blink() {
  blink(1);
}

void blink(uint8_t howManyTimes) {
  while (howManyTimes > 0) {
    digitalWrite(ledPin, HIGH);
    delay(blinkDelay);
    digitalWrite(ledPin, LOW);
    howManyTimes--;
    if (howManyTimes > 0) {
      delay(blinkDelay);
    }
  }
}
