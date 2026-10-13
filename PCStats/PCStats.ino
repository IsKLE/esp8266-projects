#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

Adafruit_SSD1306 display(128, 64, &Wire, -1);
char incoming[96];
size_t incomingLength = 0;
bool overflowed = false;
bool displayReady = false;
bool online = false;
uint32_t lastUpdate = 0;
long currentValues[6] = {0, -999, -999, -1, -1, -1};

void temperatureRow(const char *name, long temperature, uint8_t y) {
  display.setTextSize(2);
  display.setCursor(0, y);
  display.print(name);
  display.setCursor(48, y);
  if (temperature == -999) {
    display.print("--");
  } else {
    display.print((temperature + (temperature >= 0 ? 5 : -5)) / 10);
    display.print("C");
  }
}

void percentValue(long value) {
  if (value < 0) display.print("--");
  else display.print(value);
  display.print('%');
}

bool drawStats() {
  if (!displayReady) return false;
  Wire.beginTransmission(0x3C);
  if (Wire.endTransmission() != 0) return false;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(online ? "PC MONITOR  USB" : "PC OFFLINE");
  display.drawFastHLine(0, 10, 128, SSD1306_WHITE);
  temperatureRow("CPU", online ? currentValues[1] : -999, 14);
  temperatureRow("GPU", online ? currentValues[2] : -999, 34);
  display.setTextSize(1);
  display.setCursor(0, 56);
  if (online) {
    display.print("C:"); percentValue(currentValues[3]);
    display.print(" G:"); percentValue(currentValues[4]);
    display.print(" R:"); percentValue(currentValues[5]);
  } else {
    display.print("Waiting for PC...");
  }
  display.display();
  return true;
}

bool parsePacket(char *line, long *values) {
  // Validate the entire packet before changing the visible measurements.
  if (strncmp(line, "PC1,", 4) != 0) return false;
  char *cursor = line + 4;
  for (size_t i = 0; i < 6; ++i) {
    if (*cursor == '\0' || (*cursor != '-' && (*cursor < '0' || *cursor > '9'))) return false;
    errno = 0;
    char *end = nullptr;
    values[i] = strtol(cursor, &end, 10);
    if (errno == ERANGE || end == cursor) return false;
    if (i < 5) {
      if (*end != ',') return false;
      cursor = end + 1;
    } else if (*end != '\0') return false;
  }
  if (values[0] < 0) return false;
  for (size_t i = 1; i <= 2; ++i) {
    if (values[i] != -999 && (values[i] < -400 || values[i] > 1500)) return false;
  }
  for (size_t i = 3; i <= 5; ++i) {
    if (values[i] < -1 || values[i] > 100) return false;
  }
  return true;
}

void applyPacket() {
  long values[6];
  if (!parsePacket(incoming, values)) {
    Serial.println("ERR1,INVALID");
    return;
  }
  memcpy(currentValues, values, sizeof(currentValues));
  online = true;
  lastUpdate = millis();
  if (!drawStats()) {
    Serial.println("ERR1,OLED");
    return;
  }
  Serial.printf("ACK1,%ld,%ld,%ld,%ld,%ld,%ld\n", values[0], values[1], values[2], values[3], values[4], values[5]);
}

void setup() {
  Serial.begin(115200);
  Wire.begin(14, 12);
  Wire.setClock(100000);
  displayReady = display.begin(SSD1306_SWITCHCAPVCC, 0x3C, false, false);
  drawStats();
  Serial.println("READY1,PCStats");
}

void loop() {
  while (Serial.available()) {
    char value = Serial.read();
    if (value == '\n') {
      if (overflowed) Serial.println("ERR1,OVERFLOW");
      else {
        incoming[incomingLength] = '\0';
        applyPacket();
      }
      incomingLength = 0;
      overflowed = false;
    } else if (value != '\r' && !overflowed) {
      if (incomingLength + 1 < sizeof(incoming)) incoming[incomingLength++] = value;
      else overflowed = true;
    }
    yield();
  }
  if (online && millis() - lastUpdate > 5000) {
    online = false;
    drawStats();
    Serial.println("STATE1,OFFLINE");
  }
  delay(5);
}
