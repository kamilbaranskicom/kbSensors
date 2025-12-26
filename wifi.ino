// wifi.ino

#include <ESP8266NetBIOS.h>

void initWiFi() {
  Serial.print("Connecting to ");
  Serial.print(ssid);

  // connect to your local wi-fi network
  WiFi.begin(ssid, password);

  // check wi-fi is connected to wi-fi network
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected..!");
  Serial.print("Got IP: ");
  Serial.println(WiFi.localIP());
}

void initNetbiosName() {
  NBNS.begin(NETBIOSNAME);
}
