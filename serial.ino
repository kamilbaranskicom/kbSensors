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