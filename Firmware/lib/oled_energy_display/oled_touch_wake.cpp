#include "oled_touch_wake.h"

#include <Arduino.h>

#include "oled_energy_display.h"

namespace {
// Baseline is kept as Q8.8 fixed point so the slow tracking EMA can move in
// sub-LSB steps (an integer EMA on a plain uint16 stalls when |delta| < factor).
constexpr uint8_t BASELINE_FP_SHIFT = 8;
constexpr int8_t BASELINE_EMA_SHIFT = 7;  // 1/128 per sample (~6.4 s at 50 ms)
// A continuous "touch" lasting this long is treated as an environment change
// (e.g. object resting on the pad) and the baseline is re-calibrated to it.
constexpr uint32_t TOUCH_RECALIBRATION_MS = 60000;

OledTouchWake::Settings activeSettings{};
uint32_t touchBaselineFp = 0;
uint32_t displayWakeUntilMs = 0;
uint32_t lastTouchSampleMs = 0;
uint32_t continuousTouchStartMs = 0;
uint8_t consecutiveTouchHits = 0;
bool touchEventLatched = false;
volatile uint32_t touchSamples = 0;
volatile uint32_t touchThresholdHits = 0;
volatile uint32_t touchWakeEvents = 0;

uint16_t currentBaseline() {
  return static_cast<uint16_t>((touchBaselineFp + (1U << (BASELINE_FP_SHIFT - 1))) >> BASELINE_FP_SHIFT);
}

uint16_t readTouchValue() {
  int raw = touchRead(activeSettings.inputGpio);
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
  touchBaselineFp = static_cast<uint32_t>(readTouchValue()) << BASELINE_FP_SHIFT;
  displayWakeUntilMs = 0;
  lastTouchSampleMs = millis();
  continuousTouchStartMs = 0;
  consecutiveTouchHits = 0;
  touchEventLatched = false;
  touchSamples = 0;
  touchThresholdHits = 0;
  touchWakeEvents = 0;
}

void armDisplayOnTimer() {
  displayWakeUntilMs = millis() + activeSettings.displayOnTimeMs;
}

void update() {
  const uint32_t now = millis();
  if (now - lastTouchSampleMs < activeSettings.sampleIntervalMs) {
    return;
  }
  lastTouchSampleMs = now;

  const uint16_t touchValue = readTouchValue();
  const uint16_t threshold = computeTouchThreshold(currentBaseline());
  touchSamples++;
  if (touchBaselineFp == 0) {
    touchBaselineFp = static_cast<uint32_t>(touchValue) << BASELINE_FP_SHIFT;
  }

  if (touchValue > threshold) {
    consecutiveTouchHits = 0;
    touchEventLatched = false;
    continuousTouchStartMs = 0;
    // Untouched: track the resting capacitance with a slow bidirectional EMA so
    // the baseline follows environmental drift and recovers after noise dips.
    // (The previous version only adapted while touched, which ratcheted the
    // baseline down on every noise spike until real touches could no longer
    // cross the threshold.)
    const int32_t error =
        (static_cast<int32_t>(touchValue) << BASELINE_FP_SHIFT) - static_cast<int32_t>(touchBaselineFp);
    touchBaselineFp =
        static_cast<uint32_t>(static_cast<int32_t>(touchBaselineFp) + (error >> BASELINE_EMA_SHIFT));
  } else {
    touchThresholdHits++;
    if (consecutiveTouchHits < activeSettings.debounceCount) {
      consecutiveTouchHits++;
    }
    // Never adapt the baseline while touched - that is the ratchet described above.
    // But if the reading stays below threshold continuously for a very long time,
    // the environment (not a finger) changed: re-calibrate so touch can't lock out.
    if (continuousTouchStartMs == 0) {
      continuousTouchStartMs = now;
    } else if (now - continuousTouchStartMs >= TOUCH_RECALIBRATION_MS) {
      touchBaselineFp = static_cast<uint32_t>(touchValue) << BASELINE_FP_SHIFT;
      consecutiveTouchHits = 0;
      touchEventLatched = false;
      continuousTouchStartMs = 0;
    }

    if (!touchEventLatched && consecutiveTouchHits >= activeSettings.debounceCount) {
      touchEventLatched = true;
      touchWakeEvents++;
      armDisplayOnTimer();
      const bool displayOn = OledEnergyDisplay::isOn();
      if (displayOn) {
        const OledEnergyDisplay::Mode currentMode = OledEnergyDisplay::getMode();
        const OledEnergyDisplay::Mode nextMode =
            (currentMode == OledEnergyDisplay::Mode::Monitor)
                ? OledEnergyDisplay::Mode::Energy
                : OledEnergyDisplay::Mode::Monitor;
        OledEnergyDisplay::setMode(nextMode);
      } else {
        OledEnergyDisplay::turnOn();
      }
    }
  }

  const bool displayOn = OledEnergyDisplay::isOn();
  if (displayWakeUntilMs != 0 && displayOn && static_cast<int32_t>(now - displayWakeUntilMs) >= 0) {
    OledEnergyDisplay::turnOff();
  }
}

void getDiagnostics(Diagnostics* diagnostics) {
  if (diagnostics == nullptr) {
    return;
  }

  diagnostics->lastValue = readTouchValue();
  diagnostics->baseline = currentBaseline();
  diagnostics->threshold = computeTouchThreshold(currentBaseline());
  diagnostics->consecutiveHits = consecutiveTouchHits;
  diagnostics->eventLatched = touchEventLatched;
  diagnostics->displayOn = OledEnergyDisplay::isOn();
  diagnostics->samples = touchSamples;
  diagnostics->thresholdHits = touchThresholdHits;
  diagnostics->wakeEvents = touchWakeEvents;
}
}
