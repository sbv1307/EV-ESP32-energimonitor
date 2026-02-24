#include "oled_energy_display.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace {
constexpr uint8_t SCREEN_WIDTH = 128;
constexpr uint8_t SCREEN_HEIGHT = 64;
constexpr int8_t OLED_RESET = -1;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool initialized = false;
bool displayOn = true;
bool hasLastEnergy = false;
float lastEnergyKwh = 0.0f;
bool lastCharging = false;
float lastChargeEnergyKwh = 0.0f;
bool lastsmartChargingActivated = false;
char lastTimeBuf[16] = "";
float lastCurrentEnergyPrice = 0.0f;
float lastenergyPriceLlimit = 0.0f;
OledEnergyDisplay::Settings activeSettings{};
bool chargingIconVisible = true;
uint32_t chargingIconLastToggleMs = 0;
SemaphoreHandle_t displayMutex = nullptr;

bool lockDisplay() {
  if (displayMutex == nullptr) {
    displayMutex = xSemaphoreCreateMutex();
    if (displayMutex == nullptr) {
      return false;
    }
  }
  return xSemaphoreTake(displayMutex, portMAX_DELAY) == pdTRUE;
}

void unlockDisplay() {
  if (displayMutex != nullptr) {
    xSemaphoreGive(displayMutex);
  }
}

void renderEnergy(float energyKwh, bool charging, float chargeEnergyKwh, bool smartChargingActivated,
                  const char* timeBuf, float currentEnergyPrice,
                  float energyPriceLlimit);

void renderLastEnergy() {
  renderEnergy(lastEnergyKwh, lastCharging, lastChargeEnergyKwh, lastsmartChargingActivated, lastTimeBuf,
               lastCurrentEnergyPrice, lastenergyPriceLlimit);
}

void centerPrint(const String& text, int16_t y) {
  int16_t x1;
  int16_t y1;
  uint16_t width;
  uint16_t height;

  display.getTextBounds(text, 0, 0, &x1, &y1, &width, &height);
  int16_t x = (SCREEN_WIDTH - static_cast<int16_t>(width)) / 2;
  if (x < 0) {
    x = 0;
  }

  display.setCursor(x, y);
  display.print(text);
}

void renderChargingIconAt(int16_t x, int16_t y) {

  display.drawLine(x + 5, y + 0, x + 2, y + 5, SSD1306_WHITE);
  display.drawLine(x + 2, y + 5, x + 5, y + 5, SSD1306_WHITE);
  display.drawLine(x + 5, y + 5, x + 3, y + 10, SSD1306_WHITE);
  display.drawLine(x + 3, y + 10, x + 8, y + 4, SSD1306_WHITE);
  display.drawLine(x + 8, y + 4, x + 5, y + 4, SSD1306_WHITE);
}

void renderChargingIcon() {
  renderChargingIconAt(SCREEN_WIDTH - 12, 2);
}

void renderEnergy(float energyKwh, bool charging, float chargeEnergyKwh, bool smartChargingActivated,
                  const char* timeBuf, float currentEnergyPrice,
                  float energyPriceLlimit) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Smart ");
  renderChargingIconAt(36, 0);
  display.setCursor(48, 0);
  display.print(smartChargingActivated ? "ON" : "OFF");
  display.print(" ");
  if (timeBuf != nullptr && timeBuf[0] != '\0') {
    display.print(timeBuf);
  } else {
    display.print("Not set");
  }

  if (charging && chargingIconVisible) {
    renderChargingIcon();
  }

  display.setTextSize(2);
  display.setCursor(0, 22);
  display.print(energyKwh, 2);
  display.setTextSize(1);
  display.print(" kWh");

  display.setCursor(0, 46);
  display.print("Last chg: ");
  display.print(chargeEnergyKwh, 1);
  display.print(" kWh");

  display.setCursor(0, 56);
  display.print("P N/L: ");
  display.print(currentEnergyPrice, 2);
  display.print(" / ");
  display.print(energyPriceLlimit, 2);

  display.display();
}
}

namespace OledEnergyDisplay {
bool begin() {
  return begin(Settings{});
}

bool begin(const Settings& settings) {
  if (!lockDisplay()) {
    return false;
  }

  activeSettings = settings;

  Wire.begin(activeSettings.i2cSda, activeSettings.i2cScl);

  initialized = display.begin(SSD1306_SWITCHCAPVCC, activeSettings.i2cAddress);
  if (!initialized) {
    unlockDisplay();
    return false;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.display();
  displayOn = true;
  hasLastEnergy = false;
  lastEnergyKwh = 0.0f;
  lastCharging = false;
  lastChargeEnergyKwh = 0.0f;
  lastsmartChargingActivated = false;
  lastTimeBuf[0] = '\0';
  lastCurrentEnergyPrice = 0.0f;
  lastenergyPriceLlimit = 0.0f;
  unlockDisplay();
  return true;
}

void showSplash(const String& projectNameVersion, uint32_t durationMs) {
  if (!lockDisplay()) {
    return;
  }

  if (!initialized) {
    unlockDisplay();
    return;
  }

  int separatorIndex = projectNameVersion.indexOf(" - ");

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  if (separatorIndex < 0) {
    centerPrint(projectNameVersion, 20);
  } else {
    centerPrint(projectNameVersion.substring(0, separatorIndex), 20);
    centerPrint(projectNameVersion.substring(separatorIndex + 3), 34);
  }

  display.display();

  delay(durationMs);
  unlockDisplay();
}

void showEnergy(float energyKwh, bool charging, float chargeEnergyKwh, bool smartChargingActivated,
                const char* timeBuf, float currentEnergyPrice,
                float energyPriceLlimit) {
  if (!lockDisplay()) {
    return;
  }

  if (!initialized) {
    unlockDisplay();
    return;
  }

  const bool chargingStateChanged = hasLastEnergy && (lastCharging != charging);

  hasLastEnergy = true;
  lastEnergyKwh = energyKwh;
  lastCharging = charging;
  lastChargeEnergyKwh = chargeEnergyKwh;
  lastsmartChargingActivated = smartChargingActivated;
  lastCurrentEnergyPrice = currentEnergyPrice;
  lastenergyPriceLlimit = energyPriceLlimit;
  if (timeBuf != nullptr) {
    std::strncpy(lastTimeBuf, timeBuf, sizeof(lastTimeBuf) - 1);
    lastTimeBuf[sizeof(lastTimeBuf) - 1] = '\0';
  } else {
    lastTimeBuf[0] = '\0';
  }

  if (!charging || chargingStateChanged) {
    chargingIconVisible = true;
    chargingIconLastToggleMs = millis();
  }

  if (!displayOn) {
    unlockDisplay();
    return;
  }

  renderLastEnergy();
  unlockDisplay();
}

void update() {
  if (!lockDisplay()) {
    return;
  }

  if (!initialized || !displayOn || !hasLastEnergy || !lastCharging) {
    unlockDisplay();
    return;
  }

  const uint32_t now = millis();
  if (now - chargingIconLastToggleMs < activeSettings.chargingIconBlinkIntervalMs) {
    unlockDisplay();
    return;
  }

  chargingIconVisible = !chargingIconVisible;
  chargingIconLastToggleMs = now;
  renderLastEnergy();
  unlockDisplay();
}

void turnOff() {
  if (!lockDisplay()) {
    return;
  }

  if (!initialized || !displayOn) {
    unlockDisplay();
    return;
  }

  display.ssd1306_command(SSD1306_DISPLAYOFF);
  displayOn = false;
  unlockDisplay();
}

void turnOn() {
  if (!lockDisplay()) {
    return;
  }

  if (!initialized || displayOn) {
    unlockDisplay();
    return;
  }

  display.ssd1306_command(SSD1306_DISPLAYON);
  displayOn = true;

  if (hasLastEnergy) {
    renderLastEnergy();
  }
  unlockDisplay();
}

bool isOn() {
  if (!lockDisplay()) {
    return false;
  }

  const bool result = initialized && displayOn;
  unlockDisplay();
  return result;
}
}
