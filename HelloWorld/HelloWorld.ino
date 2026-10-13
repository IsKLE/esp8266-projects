#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

constexpr uint8_t kScreenWidth = 128;
constexpr uint8_t kScreenHeight = 64;
Adafruit_SSD1306 display(kScreenWidth, kScreenHeight, &Wire, -1);
bool displayReady = false;
uint8_t displayAddress = 0;
uint8_t displaySda = 0;
uint8_t displayScl = 0;

bool initializeDisplay() {
  // Integrated ESP8266 OLED boards use different I2C GPIO assignments.
  const uint8_t pinPairs[][2] = {{14, 12}, {4, 5}, {12, 14}};
  for (const auto &pins : pinPairs) {
    Wire.begin(pins[0], pins[1]);
    Wire.setClock(100000);
    delay(10);
    for (uint8_t address : {uint8_t(0x3C), uint8_t(0x3D)}) {
      Wire.beginTransmission(address);
      if (Wire.endTransmission() != 0) {
        continue;
      }
      // Keep the probed GPIOs: the library must not call default Wire.begin().
      if (!display.begin(SSD1306_SWITCHCAPVCC, address, false, false)) {
        Serial.println("OLED allocation failed");
        return false;
      }
      displayAddress = address;
      displaySda = pins[0];
      displayScl = pins[1];
      Serial.printf("OLED detected: address=0x%02X SDA=%u SCL=%u\n",
                    displayAddress, displaySda, displayScl);
      return true;
    }
    pinMode(pins[0], INPUT_PULLUP);
    pinMode(pins[1], INPUT_PULLUP);
  }
  Serial.println("OLED not detected on tested I2C pins");
  return false;
}

void drawHelloWorld() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  display.setTextSize(1);
  display.setCursor(16, 0);
  display.print("ESP8266 / ESP-12F");
  display.setTextSize(2);
  display.setCursor(34, 16);
  display.print("Hello");
  display.setCursor(34, 34);
  display.print("World");
  display.setTextSize(1);
  display.setCursor(0, 56);
  display.printf("Uptime: %lus", millis() / 1000);
  display.display();
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("ESP8266 / ESP-12F ready");
  displayReady = initializeDisplay();
  if (displayReady) {
    drawHelloWorld();
  }
}

void loop() {
  Serial.println("Hello World");
  if (displayReady) {
    drawHelloWorld();
    Serial.printf("OLED frame sent: address=0x%02X SDA=%u SCL=%u uptime=%lus\n",
                  displayAddress, displaySda, displayScl, millis() / 1000);
  }
  delay(1000);
}
