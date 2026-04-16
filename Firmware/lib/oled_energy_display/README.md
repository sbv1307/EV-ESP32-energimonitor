# OLED Energy Display Library

This library exposes a small, stable API for OLED display + touch wake behavior.
Use `OledLibrary` as the primary integration entry point.

## Public API

Headers intended for application code:

- `oled_library.h`
- `oled_energy_display.h`
- `oled_touch_wake.h`

Main integration API:

- `bool OledLibrary::begin();`
- `bool OledLibrary::begin(const OledLibrary::Settings& settings);`
- `void OledLibrary::update();`
- `bool OledLibrary::startBackgroundUpdater(uint32_t intervalMs = 20, uint32_t stackSizeWords = 2048, uint32_t priority = 1, int8_t coreId = 1);`
- `void OledLibrary::stopBackgroundUpdater();`
- `bool OledLibrary::isBackgroundUpdaterRunning();`

Display data API:

- `void OledEnergyDisplay::showEnergy(...);`
- `void OledEnergyDisplay::showMonitorLine(const char* text);`
- `void OledEnergyDisplay::clearMonitorLines();`
- `void OledEnergyDisplay::setMode(OledEnergyDisplay::Mode mode);`
- `void OledEnergyDisplay::turnOn();`
- `void OledEnergyDisplay::turnOff();`
- `bool OledEnergyDisplay::isOn();`

## Quick Start (defaults)

```cpp
#include "oled_library.h"

void setup() {
  if (!OledLibrary::begin()) {
    // handle init error
  }
}

void loop() {
  OledLibrary::update();
}
```

## Runtime Integration

### `OledLibrary::update()`

Call `OledLibrary::update()` once per loop iteration.

What it handles internally:

- Touch sampling/debounce for wake detection.
- Auto on/off behavior for the OLED display.
- Charging icon blink timing updates.

Recommended pattern:

```cpp
void loop() {
  OledLibrary::update();

  // your app logic (sensors/network/state machine)
  // optional short delay/yield
}
```

Notes:

- Keep calling `OledLibrary::update()` even when you are not sending new energy data.
- Do not call it from interrupts.
- If your loop has long blocking work, call `OledLibrary::update()` before and after that work when possible.

### Optional FreeRTOS Background Updater (ESP32)

If your `loop()` can block or sleep for long periods, you can run OLED updates in a dedicated task.

```cpp
void setup() {
  OledLibrary::Settings settings;
  OledLibrary::begin(settings);

  // intervalMs, stackSizeWords, priority, coreId
  OledLibrary::startBackgroundUpdater(20, 1424, 1, 1);
}

void loop() {
  // no need to call OledLibrary::update() when background updater is running
}
```

Notes:

- Background updater is optional and ESP32-only.
- Calling `startBackgroundUpdater(...)` multiple times is safe; it will keep one task instance.
- Call `stopBackgroundUpdater()` before shutdown/reconfiguration if needed.

### `OledEnergyDisplay::showEnergy(...)`

Signature:

```cpp
void showEnergy(float energyKwh,
                bool charging = false,
                float chargeEnergyKwh = 0.0f,
                bool smartChargingActivated = false,
                const char* timeBuf = nullptr,
                float currentEnergyPrice = 0.0f,
                float energyPriceLlimit = 0.0f);
```

Parameter meaning:

- `energyKwh`: Main total energy value shown in large text.
- `charging`: Current charging state; enables charging icon behavior.
- `chargeEnergyKwh`: Last/active session energy shown as `Last chg`.
- `smartChargingActivated`: Displays `Smart ON/OFF` status in header.
- `timeBuf`: End time string (format `"HH:MM"`). Use `nullptr` or empty string to show `Not set`.
- `currentEnergyPrice`: Price shown as the first value in `P N/L`.
- `energyPriceLlimit`: Price shown as the second value in `P N/L`.

Recommended call pattern:

```cpp
void loop() {
  OledLibrary::update();

  static uint32_t lastEnergyUpdateMs = 0;
  const uint32_t now = millis();
  if (now - lastEnergyUpdateMs < 5000) {
    return;
  }
  lastEnergyUpdateMs = now;

  char timeBuf[6] = "12:30";
  OledEnergyDisplay::showEnergy(26.10f,
                                true,
                                5.0f,
                                true,
                                timeBuf,
                                1.23f,
                                0.89f);
}
```

Tips:

- Update display data on state/value changes or on a fixed interval (for example 1-5 s).
- `showEnergy(...)` is safe to call even if display is currently off; latest values are cached and shown again after wake.
- Display API operations are mutex-protected to support concurrent calls from multiple tasks.

## Custom Settings (recommended for portability)

```cpp
#include "oled_library.h"

void setup() {
  OledLibrary::Settings settings;

  // Display settings
  settings.energyDisplay.i2cSda = 21;
  settings.energyDisplay.i2cScl = 22;
  settings.energyDisplay.i2cAddress = 0x3C;
  settings.energyDisplay.chargingIconBlinkIntervalMs = 400;

  // Touch settings
  settings.touchWake.inputGpio = 4;
  settings.touchWake.displayOnTimeMs = 30000;
  settings.touchWake.sampleIntervalMs = 50;
  settings.touchWake.minDelta = 12;
  settings.touchWake.debounceCount = 2;

  // Optional boot splash
  settings.showSplashOnBoot = true;
  settings.splashText = "OLED test - V1.0.0";
  settings.splashDurationMs = 5000;
  settings.turnOffAfterSplash = true;

  if (!OledLibrary::begin(settings)) {
    // handle init error
  }
}
```

## Compile-time Default Overrides

You can override default values before including the headers.

Display defaults (`oled_energy_display.h`):

- `OLED_ENERGY_DISPLAY_DEFAULT_I2C_SDA`
- `OLED_ENERGY_DISPLAY_DEFAULT_I2C_SCL`
- `OLED_ENERGY_DISPLAY_DEFAULT_I2C_ADDRESS`
- `OLED_ENERGY_DISPLAY_DEFAULT_CHARGING_ICON_BLINK_MS`
- `OLED_ENERGY_DISPLAY_DEFAULT_MONITOR_LINE_CAPACITY`
- `OLED_ENERGY_DISPLAY_DEFAULT_MONITOR_CHARS_PER_LINE`
- `OLED_ENERGY_DISPLAY_DEFAULT_MONITOR_FREEZE_MS`
- `OLED_ENERGY_DISPLAY_DEFAULT_MONITOR_SCROLL_STEP_MS`

## Monitor Mode

The OLED can now buffer a rolling set of short text lines for boot/status/error/debug output.

Behavior:

- The monitor buffer keeps the last configured number of lines.
- Each new line is shown immediately and frozen for `freezeDurationMs`.
- If the OLED is currently showing the energy screen, a new monitor line automatically switches the display to monitor mode.
- Each new monitor line resets the monitor scroll sequence so the buffered lines are shown again from the beginning of the scroll animation.
- After the freeze period, the OLED starts at the oldest line, adds one line per `scrollStepMs`, and then scrolls down one line at a time until the newest lines are visible.
- If no new monitor line is added for `OLED_TOUCH_WAKE_DEFAULT_DISPLAY_ON_TIME_MS`, the OLED automatically switches back to the energy display.
- A touch on `OLED_TOUCH_WAKE_DEFAULT_INPUT_GPIO` toggles between monitor mode and energy mode when the OLED is already on and showing one of those modes.
- If the OLED is off, the same touch input only wakes the display and keeps the current mode unchanged.
- Each touch is latched so one press produces one mode switch.

Notes:

- The monitor inactivity timeout currently uses `OLED_TOUCH_WAKE_DEFAULT_DISPLAY_ON_TIME_MS`.
- With the default value, the monitor returns to the energy screen after 30 seconds without a new `showMonitorLine(...)` call.

Configuration is part of `OledEnergyDisplay::Settings`:

```cpp
OledLibrary::Settings settings;
settings.energyDisplay.initialMode = OledEnergyDisplay::Mode::Monitor;
settings.energyDisplay.monitor.lineCapacity = 10;
settings.energyDisplay.monitor.charsPerLine = 21;
settings.energyDisplay.monitor.freezeDurationMs = 5000;
settings.energyDisplay.monitor.scrollStepMs = 1000;
settings.turnOffAfterSplash = false;
```

Adding monitor lines:

```cpp
OledEnergyDisplay::showMonitorLine("Boot OK");
OledEnergyDisplay::showMonitorLine("WiFi reconnect");
```

If you need to force a mode explicitly:

```cpp
OledEnergyDisplay::setMode(OledEnergyDisplay::Mode::Monitor);
OledEnergyDisplay::setMode(OledEnergyDisplay::Mode::Energy);
```

Touch defaults (`oled_touch_wake.h`):

- `OLED_TOUCH_WAKE_DEFAULT_INPUT_GPIO`
- `OLED_TOUCH_WAKE_DEFAULT_DISPLAY_ON_TIME_MS`
- `OLED_TOUCH_WAKE_DEFAULT_SAMPLE_INTERVAL_MS`
- `OLED_TOUCH_WAKE_DEFAULT_MIN_DELTA`
- `OLED_TOUCH_WAKE_DEFAULT_DEBOUNCE_COUNT`

## Porting Notes

- Prefer `OledLibrary::Settings` instead of hard-coded project constants.
- Keep app logic in `src/`, keep library internals private to `.cpp` files.
- Call `OledLibrary::update()` regularly in the main loop for touch wake + blink updates.

## Migration Snippet

```cpp
#include "oled_library.h"

void setup() {
  OledLibrary::Settings settings;

  settings.touchWake.inputGpio = 4;
  settings.touchWake.displayOnTimeMs = 30000;
  settings.touchWake.sampleIntervalMs = 50;
  settings.touchWake.minDelta = 12;
  settings.touchWake.debounceCount = 2;

  settings.showSplashOnBoot = true;
  settings.splashText = "OLED test - V1.0.0";
  settings.splashDurationMs = 5000;
  settings.turnOffAfterSplash = true;

  if (!OledLibrary::begin(settings)) {
    // handle init error
  }
}

void loop() {
  OledLibrary::update();
}
```

