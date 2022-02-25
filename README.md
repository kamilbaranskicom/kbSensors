# kbSensors
- Tiny monitor station for one DHT11 and many DS18B20 sensors for ESP8266 board
- Outputs data at http://kbsensors/ , http://kbsensors/xml , http://kbsensors/txt . (https is not available for now.)

## INSTALLATION:
- Use [this tool](https://github.com/earlephilhower/arduino-esp8266littlefs-plugin) for Arduino IDE to flash static files onto the ESP (or try `REMOTE_HELPER_SERVER` instead)
- copy `wifiConfig_sample.h` to `wifiConfig.h` and enter your ssid/password there.
