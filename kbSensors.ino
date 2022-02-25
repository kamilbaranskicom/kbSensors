const char PROGRAMNAME[] = "KB Sensors station.";

const char PROGRAMVERSION[] = "0.20220225";

const char PROGRAMMANUAL[] =
  "// tiny monitor station for one DHT11 and many DS18B20 sensors\n"
  "// outputs data at http://kbsensors/ , http://kbsensors/xml , http://kbsensors/txt\n"
  "// https is not available for now.";


#define REMOTEHELPERSERVER "http://192.168.50.3/kbSensors/"


#include "DHTStable.h"
#define DHT11_PIN 4 // D2
DHTStable DHT;

#include "wifiConfig.h"
// TODO: config using www?? naah.

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
ESP8266WebServer server(80);
#include <uri/UriRegex.h>

// friendly "kbsensors" name
#define NETBIOSNAME "kbsensors"
#include <ESP8266NetBIOS.h>

// to update over the air
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// the sensors are slow; you can read them not more often than 1s.
const unsigned int updateInterval = 1000;
unsigned long previousMillis = 0;

#include <OneWire.h>
#include <DallasTemperature.h>
#define DS18B20_PIN 5 // D1

OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);  // Przekazania informacji do biblioteki
DeviceAddress Thermometer;

// we write these tables when reading sensors
// and read these tables when sending results to user
uint8_t sensorResultCount = 0;
#define MAXRESULTSCOUNT 255
String sensorAddresses[MAXRESULTSCOUNT];  // String, because we use also "DHTtemp" and "DHThumi"; that's also good for easy adding future sensors
String friendlyNames[MAXRESULTSCOUNT];
float values[MAXRESULTSCOUNT];
float compensation[MAXRESULTSCOUNT];
String valueType[MAXRESULTSCOUNT];

// for valueType[]
const String TYPE_PERCENT = "%";
const String TYPE_CDEGREE = "°C";

// this is the sensors database, it is (will be) stored on the device (in EEPROM). Compensation and friendly names.
String sensorsDBAddresses[MAXRESULTSCOUNT];
String sensorsDBFriendlyNames[MAXRESULTSCOUNT];
float sensorsDBCompensation[MAXRESULTSCOUNT];
uint8_t sensorsDBCount = 0;

void setup() {
  // put your setup code here, to run once:
  initSerial();
  initWiFi();
  initOTA();
  initNetbiosName();
  initWebserver();
  loadSensorsDB();
  sensors.begin(); // ds18b20 init
}

void initSerial() {
  Serial.begin(9600);
  Serial.setTimeout(50);
  showAbout();
  Serial.println("Serial initialized!");
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

  sensorsDBCount = 4;
}

void saveSensorsDB() {
  // TODO
}

void showAbout() {
  Serial.println("\n\n");
  Serial.println(PROGRAMNAME);
  Serial.println(PROGRAMVERSION);
  Serial.println(PROGRAMMANUAL);
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
  server.on(UriRegex("^\\/(kbSensors\\.css|kbSensors\\.js|kbSensors\\.svg)$"), []() {
    handleFileDownload(server.pathArg(0));
  });

  // special pages
  server.on("/edit", handleEdit);
  server.on("/reboot", handleReboot);

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
  ArduinoOTA.handle();
}

void handleFileDownload(String file) {
  // TODO: currently we use css and js file from remote server on REMOTEHELPERSERVER (http://192.168.50.3/kbSensors/),
  // but we should move these files locally someday.
  server.sendHeader("Location", String(REMOTEHELPERSERVER) + file, true);
  server.send(302, "text/plain", "");
}

void handleClientAskingAboutSensors(String desiredFormat) {
  updateSensorsValues();
  if ((desiredFormat == "txt") || (desiredFormat == "text")) {
    server.send(200, "text/plain", sendTXT());
  } else if (desiredFormat == "xml") {
    server.send(200, "text/xml", sendXML());
  } else {
    server.send(200, "text/html", sendHTML());
  }
}

void handleReboot() {
  Serial.println("Reboot request!");
  server.send(200, "text/html", F("<html><head><meta http-equiv=\"refresh\" content=\"10;url=/\"></head><body>Reboot in progress...</body></html>"));
  delay(2000); // 2s to allow user to download the page
  ESP.restart();
}

void handleEdit() {
  String sensorAddress = "";
  String sensorNewFriendlyName = "";
  float sensorNewCompensation = 0;

  for (uint8_t i = 0; i < server.args(); i++) {
    if (server.argName(i) == "address") {
      // TODO: validate input
      sensorAddress = server.arg(i);
    } else if (server.argName(i) == "friendlyName") {
      // TODO: validate input
      sensorNewFriendlyName = server.arg(i);
    } else if (server.argName(i) == "compensation") {
      // TODO: validate input
      sensorNewCompensation = atof(server.arg(i).c_str());
    }
  }

  if ((sensorAddress != "") && (sensorNewFriendlyName != "")) {
    Serial.println("New name of sensor " + sensorAddress + " is " + sensorNewFriendlyName + "; compensation = " + (String)sensorNewCompensation + ".");
  } else {
    Serial.println("Incorrect arguments.");
    server.send(403, "text/plain", "Arguments error. Use ?address=123456789ABCDEF0&friendlyName=new_name&compensation=1.5");
    return;
  }

  if (editSensor(sensorAddress, sensorNewFriendlyName, sensorNewCompensation)) {
    server.send(200, "text/plain", "Sensor saved!");
    Serial.println("Sensor saved.");
  } else {
    server.send(403, "text/plain", "Error while saving sensor.");
    Serial.println("Error while saving sensor.");
  }
}

boolean editSensor(String sensorAddress, String sensorNewFriendlyName, float sensorNewCompensation) {
  uint8_t sensorDBIndex = getSensorsDBIndex(sensorAddress);

  // sensor doesn't exists
  if (sensorDBIndex == 255) {
    sensorDBIndex = sensorsDBCount;
    sensorsDBCount++;
  }

  sensorsDBAddresses[sensorDBIndex] = sensorAddress;
  sensorsDBFriendlyNames[sensorDBIndex] = sensorNewFriendlyName;
  sensorsDBCompensation[sensorDBIndex] = sensorNewCompensation;

  saveSensorsDB();

  return true;
}

String sendHTML() {
  String ptr = "<!DOCTYPE html>\n";
  ptr += "<html>\n";
  ptr += "<head>\n";
  ptr += "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "  <meta charset=\"UTF-8\">\n";
  ptr += "  <title>" + (String)PROGRAMNAME + "</title>\n";
  ptr += "  <link rel=\"stylesheet\" href=\"/kbSensors.css\">\n";
  ptr += "  <script src=\"/kbSensors.js\" defer></script>\n";
  ptr += "  <link rel=\"icon\" href=\"/kbSensors.svg\" type=\"image/svg+xml\">\n";
  ptr += "</head>\n";
  ptr += "<body><form><div id=\"webpage\">\n";
  ptr += "<h1>" + (String)PROGRAMNAME + "</h1>\n";

  if (sensorResultCount > 0) {
    ptr += "<table>\n";
    for (uint8_t deviceNumber = 0; deviceNumber < sensorResultCount; deviceNumber++) {
      ptr += "<tr>\n";
      ptr += "  <td title=" + (String)sensorAddresses[deviceNumber] + ">" + (String)friendlyNames[deviceNumber] + "\n";
      ptr += "  <td title=\"" + (String)compensation[deviceNumber] + "\">" + (String)(values[deviceNumber] + compensation[deviceNumber]) + valueType[deviceNumber] + "\n";
      ptr += "</tr>\n";
    }
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
  String ptr = "";

  for (uint8_t deviceNumber = 0; deviceNumber < sensorResultCount; deviceNumber++) {
    // TODO: replace spaces with "_"
    ptr += (String)friendlyNames[deviceNumber] + ".value " + (String)(values[deviceNumber] + compensation[deviceNumber]) + "\n";
  }
  return ptr;
}

void updateSensorsValues() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= updateInterval) {
    previousMillis = currentMillis;

    // we can reset the ResultCount.
    sensorResultCount = 0;
    updateDHTValues();
    updateDS18B20Values();
    emptyRestOfTheArray();
  }
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

    if (temperature > -127) { // valid result
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
