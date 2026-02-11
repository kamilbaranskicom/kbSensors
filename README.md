# kbSensors
- Tiny monitor station for one DHT11, one CCS811 and many DS18B20 sensors for ESP8266 board
- Outputs data at http://kbsensors/ (optional ?refresh=&lt;seconds&gt;), http://kbsensors/xml , http://kbsensors/txt , http://kbsensors/json. (https is not available for now.)
- Sends data to MQTT
- Can get temperature & humidity from local sensors or another kbSensors instance or MQTT for CCS811 compensation

## INSTALLATION:
- Use [this tool](https://github.com/earlephilhower/arduino-esp8266littlefs-plugin) for Arduino IDE to flash static files onto the ESP (or try `REMOTE_HELPER_SERVER` instead)
- First time: login to wifi kbSensors-xxxx, go to 192.168.4.1 and set the wifi network ssid+pass (kbSensors doesn't work as an AP except for the setup mode).

## CRAZY SCREENSHOTS:
<img src="screenshots/kbSensors www.png">
<img src="screenshots/kbSensors xml.png">
<img src="screenshots/kbSensors munin.png">
<img src="screenshots/kbSensors homeassistant.png">