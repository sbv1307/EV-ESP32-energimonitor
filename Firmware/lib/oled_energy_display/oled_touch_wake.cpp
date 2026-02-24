#include "oled_touch_wake.h"

#include <Arduino.h>

#include "oled_energy_display.h"

namespace {
OledTouchWake::Settings activeSettings{};
uint16_t touchBaseline = 0;
uint32_t displayWakeUntilMs = 0;
uint32_t lastTouchSampleMs = 0;
uint8_t consecutiveTouchHits = 0;

uint16_t readTouchValue() {
  int raw = touchRead(activeSettings.inputPin);
  if (raw < 0) {
    return 0;
  }
  if (raw > 0xFFFF) {
    return 0xFFFF;
  }
  return static_cast<uint16_t>(raw);
}

uint16_t computeTouchThreshold(uint16_t baseline) {
  uint16_t adaptiveDelta = baseline / 10;
  if (adaptiveDelta < activeSettings.minDelta) {
    adaptiveDelta = activeSettings.minDelta;
  }

  if (baseline <= adaptiveDelta) {
    return 0;
  }
  return baseline - adaptiveDelta;
}
}

namespace OledTouchWake {
void begin() {
  begin(Settings{});
}

void begin(const Settings& settings) {
  activeSettings = settings;
  touchBaseline = readTouchValue();
  displayWakeUntilMs = 0;
  lastTouchSampleMs = millis();
  consecutiveTouchHits = 0;
}

void update() {
  const uint32_t now = millis();
  if (now - lastTouchSampleMs < activeSettings.sampleIntervalMs) {
    return;
  }
  lastTouchSampleMs = now;

  const uint16_t touchValue = readTouchValue();
  if (touchBaseline == 0) {
    touchBaseline = touchValue;
  }

  if (touchValue > computeTouchThreshold(touchBaseline)) {
    consecutiveTouchHits = 0;
    touchBaseline = static_cast<uint16_t>((touchBaseline * 15U + touchValue) / 16U);
  } else {
    if (consecutiveTouchHits < activeSettings.debounceCount) {
      consecutiveTouchHits++;
    }

    if (consecutiveTouchHits >= activeSettings.debounceCount) {
      displayWakeUntilMs = now + activeSettings.displayOnTimeMs;
      if (!OledEnergyDisplay::isOn()) {
        OledEnergyDisplay::turnOn();
      }
    }
  }

  const bool displayOn = OledEnergyDisplay::isOn();
  if (displayOn && static_cast<int32_t>(now - displayWakeUntilMs) >= 0) {
    OledEnergyDisplay::turnOff();
  }
}
}
