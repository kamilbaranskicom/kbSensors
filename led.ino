/*
   LED for blinking
*/
uint16_t blinkDelay = 100;  // 0.1 s
const uint8_t ledPin = LED_BUILTIN;


void initLED() {
  pinMode(ledPin, OUTPUT);
  blink(2);
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
