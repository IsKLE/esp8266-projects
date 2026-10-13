#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "oled-image.h"

constexpr uint8_t kOledAddress = 0x3C;
constexpr uint8_t kOledSda = 14;
constexpr uint8_t kOledScl = 12;
Adafruit_SSD1306 display(128, 64, &Wire, -1);
bool imageReady = false;

void setup() {
  Serial.begin(115200);
  delay(200);
  Wire.begin(kOledSda, kOledScl);
  Wire.setClock(100000);
  Wire.beginTransmission(kOledAddress);
  if (Wire.endTransmission() != 0) {
    Serial.println("IMAGE ERROR: OLED not detected");
    return;
  }
  if (!display.begin(SSD1306_SWITCHCAPVCC, kOledAddress, false, false)) {
    Serial.println("IMAGE ERROR: framebuffer allocation failed");
    return;
  }
  display.clearDisplay();
  display.drawBitmap(0, 0, oledImage, 128, 64, SSD1306_WHITE);
  display.display();
  imageReady = true;
}

void loop() {
  if (imageReady) {
    // Confirm that the physical display remains connected, without text overlays.
    Wire.beginTransmission(kOledAddress);
    if (Wire.endTransmission() == 0) {
      Serial.println("IMAGE displayed: 128x64 bytes=1024 address=0x3C SDA=14 SCL=12");
    } else {
      Serial.println("IMAGE ERROR: OLED no longer responds");
    }
  } else {
    Serial.println("IMAGE ERROR: image not initialized");
  }
  delay(1000);
}
