#include <ESP8266SSDP.h>

String getUUID() { return "uuid:kbSensors-" + String(ESP.getChipId(), HEX); }

void initSSDP() {
  SSDP.setSchemaURL("description.xml");
  SSDP.setHTTPPort(80);
  SSDP.setName((String)deviceName); // np. "kbSensors Salon"
  SSDP.setDeviceType("upnp:rootdevice");
  static String serial = String(ESP.getChipId(), HEX);
  SSDP.setSerialNumber(serial.c_str());
  SSDP.setURL("/");
  SSDP.setModelName("kbSensors");
  SSDP.setModelNumber("1");
  SSDP.setManufacturer("Kamil Baranski");
  SSDP.setManufacturerURL("https://github.com/kamilbaranskicom/kbSensors");

  SSDP.begin();
}

String getSSDPDescriptionString() {
  String xml = "<?xml version=\"1.0\"?>"
               "<root xmlns=\"urn:schemas-upnp-org:device-1-0\">"
               "<specVersion>"
               "<major>1</major>"
               "<minor>0</minor>"
               "</specVersion>"
               "<device>"
               "<deviceType>urn:schemas-upnp-org:device:Basic:1</deviceType>"
               //"<deviceType>urn:schemas-kb:device:kbSensors:1</deviceType>"
               "<friendlyName>" +
               (String)deviceName +
               "</friendlyName>"
               "<manufacturer>kb</manufacturer>"
               "<modelName>kbSensors</modelName>"
               "<modelNumber>1</modelNumber>"
               "<serialNumber>" +
               String(ESP.getChipId()) +
               "</serialNumber>"
               "<UDN>uuid:kbSensors-" +
               String(ESP.getChipId(), HEX) +
               "</UDN>"
               "<presentationURL>http://" +
               WiFi.localIP().toString() +
               "/</presentationURL>"
               "</device>"
               "</root>";

  return xml;
}
