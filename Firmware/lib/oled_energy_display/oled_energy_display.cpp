#include "oled_energy_display.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <algorithm>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace {
constexpr uint8_t SCREEN_WIDTH = 128;
constexpr uint8_t SCREEN_HEIGHT = 64;
constexpr int8_t OLED_RESET = -1;
constexpr uint8_t MONITOR_MAX_LINES = 16;
constexpr uint8_t MONITOR_MAX_CHARS = 32;
constexpr uint8_t MONITOR_LINE_HEIGHT = 8;
constexpr uint8_t MONITOR_VISIBLE_LINES = SCREEN_HEIGHT / MONITOR_LINE_HEIGHT;

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
OledEnergyDisplay::Mode activeMode = OledEnergyDisplay::Mode::Energy;
bool chargingIconVisible = true;
uint32_t chargingIconLastToggleMs = 0;
char monitorLines[MONITOR_MAX_LINES][MONITOR_MAX_CHARS + 1] = {};
uint8_t monitorLineCount = 0;
uint8_t monitorWriteIndex = 0;
uint32_t monitorFreezeUntilMs = 0;
uint32_t monitorLastScrollStepMs = 0;
uint32_t monitorScrollStep = 0;
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

uint8_t getMonitorLineCapacity() {
  return std::max<uint8_t>(1, std::min<uint8_t>(activeSettings.monitor.lineCapacity, MONITOR_MAX_LINES));
}

uint8_t getMonitorCharsPerLine() {
  return std::max<uint8_t>(1, std::min<uint8_t>(activeSettings.monitor.charsPerLine, MONITOR_MAX_CHARS));
}

uint8_t getMonitorVisibleLineCount() {
  return std::min<uint8_t>(getMonitorLineCapacity(), MONITOR_VISIBLE_LINES);
}

uint8_t getMonitorOldestIndex() {
  const uint8_t capacity = getMonitorLineCapacity();
  if (monitorLineCount < capacity) {
    return 0;
  }
  return monitorWriteIndex % capacity;
}

const char* getMonitorLineAt(uint8_t logicalIndex) {
  if (logicalIndex >= monitorLineCount) {
    return "";
  }

  const uint8_t capacity = getMonitorLineCapacity();
  const uint8_t oldestIndex = getMonitorOldestIndex();
  const uint8_t physicalIndex = static_cast<uint8_t>((oldestIndex + logicalIndex) % capacity);
  return monitorLines[physicalIndex];
}

void renderEnergy(float energyKwh, bool charging, float chargeEnergyKwh, bool smartChargingActivated,
                  const char* timeBuf, float currentEnergyPrice,
                  float energyPriceLlimit);

void renderLastEnergy() {
  renderEnergy(lastEnergyKwh,
               lastCharging,
               lastChargeEnergyKwh,
               lastsmartChargingActivated,
               lastTimeBuf,
               lastCurrentEnergyPrice,
               lastenergyPriceLlimit);
}

void renderMonitorWindow(uint8_t startIndex, uint8_t visibleCount) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  for (uint8_t lineIndex = 0; lineIndex < visibleCount; ++lineIndex) {
    display.setCursor(0, lineIndex * MONITOR_LINE_HEIGHT);
    display.print(getMonitorLineAt(startIndex + lineIndex));
  }

  display.display();
}

void renderMonitorLatestWindow() {
  const uint8_t visibleLines = getMonitorVisibleLineCount();
  const uint8_t startIndex = (monitorLineCount > visibleLines)
                                 ? static_cast<uint8_t>(monitorLineCount - visibleLines)
                                 : 0;
  renderMonitorWindow(startIndex, std::min<uint8_t>(visibleLines, monitorLineCount));
}

void renderMonitorAnimatedWindow() {
  if (monitorLineCount == 0) {
    display.clearDisplay();
    display.display();
    return;
  }

  const uint8_t visibleLines = getMonitorVisibleLineCount();
  const uint8_t linesOnScreen = std::min<uint8_t>(visibleLines, monitorLineCount);
  const uint32_t fillSteps = (linesOnScreen > 0) ? (linesOnScreen - 1U) : 0U;

  if (monitorScrollStep <= fillSteps) {
    renderMonitorWindow(0, static_cast<uint8_t>(monitorScrollStep + 1U));
    return;
  }

  const uint32_t scrollOffset = monitorScrollStep - fillSteps;
  const uint8_t maxStartIndex = (monitorLineCount > linesOnScreen)
                                    ? static_cast<uint8_t>(monitorLineCount - linesOnScreen)
                                    : 0;
  const uint8_t startIndex = static_cast<uint8_t>(std::min<uint32_t>(scrollOffset, maxStartIndex));
  renderMonitorWindow(startIndex, linesOnScreen);
}

void renderActiveMode() {
  if (activeMode == OledEnergyDisplay::Mode::Monitor) {
    const uint32_t now = millis();
    if (monitorFreezeUntilMs != 0 && static_cast<int32_t>(now - monitorFreezeUntilMs) < 0) {
      renderMonitorLatestWindow();
    } else {
      renderMonitorAnimatedWindow();
    }
    return;
  }

  if (hasLastEnergy) {
    renderLastEnergy();
    return;
  }

  display.clearDisplay();
  display.display();
}

void resetMonitorScrollWindow() {
  monitorFreezeUntilMs = millis() + activeSettings.monitor.freezeDurationMs;
  monitorLastScrollStepMs = 0;
  monitorScrollStep = 0;
}

void storeMonitorLine(const char* text) {
  const uint8_t capacity = getMonitorLineCapacity();
  const uint8_t charsPerLine = getMonitorCharsPerLine();
  char* destination = monitorLines[monitorWriteIndex];
  uint8_t writePos = 0;

  if (text != nullptr) {
    while (*text != '\0' && writePos < charsPerLine) {
      char current = *text++;
      if (current == '\r' || current == '\n' || current == '\t') {
        current = ' ';
      }
      destination[writePos++] = current;
    }
  }

  destination[writePos] = '\0';
  monitorWriteIndex = static_cast<uint8_t>((monitorWriteIndex + 1) % capacity);
  if (monitorLineCount < capacity) {
    ++monitorLineCount;
  }
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
  display.print(currentEnergyPrice, 3);
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
  activeMode = activeSettings.initialMode;

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
  chargingIconVisible = true;
  chargingIconLastToggleMs = 0;
  monitorLineCount = 0;
  monitorWriteIndex = 0;
  monitorFreezeUntilMs = 0;
  monitorLastScrollStepMs = 0;
  monitorScrollStep = 0;
  for (uint8_t lineIndex = 0; lineIndex < MONITOR_MAX_LINES; ++lineIndex) {
    monitorLines[lineIndex][0] = '\0';
  }
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

  const int separatorIndex = projectNameVersion.indexOf(" - ");

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

  if (!displayOn || activeMode != Mode::Energy) {
    unlockDisplay();
    return;
  }

  renderLastEnergy();
  unlockDisplay();
}

void showMonitorLine(const char* text) {
  if (!lockDisplay()) {
    return;
  }

  if (!initialized) {
    unlockDisplay();
    return;
  }

  storeMonitorLine(text);
  resetMonitorScrollWindow();

  if (displayOn && activeMode == Mode::Monitor) {
    renderMonitorLatestWindow();
  }

  unlockDisplay();
}

void showMonitorLine(const String& text) {
  showMonitorLine(text.c_str());
}

void clearMonitorLines() {
  if (!lockDisplay()) {
    return;
  }

  monitorLineCount = 0;
  monitorWriteIndex = 0;
  monitorFreezeUntilMs = 0;
  monitorLastScrollStepMs = 0;
  monitorScrollStep = 0;
  for (uint8_t lineIndex = 0; lineIndex < MONITOR_MAX_LINES; ++lineIndex) {
    monitorLines[lineIndex][0] = '\0';
  }

  if (initialized && displayOn && activeMode == Mode::Monitor) {
    display.clearDisplay();
    display.display();
  }

  unlockDisplay();
}

void setMode(Mode mode) {
  if (!lockDisplay()) {
    return;
  }

  activeMode = mode;
  if (initialized && displayOn) {
    renderActiveMode();
  }
  unlockDisplay();
}

Mode getMode() {
  if (!lockDisplay()) {
    return activeMode;
  }

  const Mode result = activeMode;
  unlockDisplay();
  return result;
}

void update() {
  if (!lockDisplay()) {
    return;
  }

  if (!initialized || !displayOn) {
    unlockDisplay();
    return;
  }

  const uint32_t now = millis();
  if (activeMode == Mode::Monitor) {
    if (monitorFreezeUntilMs != 0 && static_cast<int32_t>(now - monitorFreezeUntilMs) < 0) {
      unlockDisplay();
      return;
    }

    if (monitorLineCount == 0) {
      unlockDisplay();
      return;
    }

    const uint32_t scrollStepMs = std::max<uint32_t>(1U, activeSettings.monitor.scrollStepMs);
    if (monitorLastScrollStepMs != 0 && now - monitorLastScrollStepMs < scrollStepMs) {
      unlockDisplay();
      return;
    }

    monitorLastScrollStepMs = now;
    renderMonitorAnimatedWindow();

    const uint8_t visibleLines = std::min<uint8_t>(getMonitorVisibleLineCount(), monitorLineCount);
    if (visibleLines > 0) {
      const uint32_t maxScrollStep = (visibleLines - 1U) +
                                     ((monitorLineCount > visibleLines)
                                          ? static_cast<uint32_t>(monitorLineCount - visibleLines)
                                          : 0U);
      if (monitorScrollStep < maxScrollStep) {
        ++monitorScrollStep;
      }
    }

    unlockDisplay();
    return;
  }

  if (!hasLastEnergy || !lastCharging) {
    unlockDisplay();
    return;
  }

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
  renderActiveMode();
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
