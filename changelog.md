# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog],
and this project adheres to [Semantic Versioning].

## [Unreleased]

## [V4.0.0] - 2026-04-25

### Changed

- Enabled real pulse interrupt input by setting `PULSE_INPUT_GPIO` to `33` in `Firmware/lib/config/config.h`.

### Removed

- Removed obsolete ISR pulse simulation module `Firmware/lib/testPulse`.
- Removed `startPulseInputIsrTestTask()` integration from `Firmware/src/main.cpp`.
- Removed `PULSE_ISR_TEST_TASK_STACK_SIZE` from `Firmware/lib/globals/globals.h`.

## [V3.2.0] - 2026-04-25

### Added

- Issue [#10 Turn on 'energy display' when car connects](https://github.com/sbv1307/EV-ESP32-energimonitor/issues/10)
- Subscribed MQTT client to TeslaMate topic `teslamate/cars/1/plugged_in`.
- Added MQTT RX handling so payload `true`/`True` on `teslamate/cars/1/plugged_in` triggers `OledTouchWake::armDisplayOnTimer()`.
- On `plugged_in=true`, OLED is now turned on (if currently off) and switched to Energy mode.

### Changed

- Reused the existing touch-wake display-on timer for MQTT-triggered wake behavior, so on-time follows `OLED_TOUCH_WAKE_DEFAULT_DISPLAY_ON_TIME_MS` (or configured touch-wake override).

## [V3.1.0] - 2026-04-23

### Added

- Issue [#7 Add boot diagnostics to boot telemetry to send to google sheets](https://github.com/sbv1307/EV-ESP32-energimonitor/issues/7)
- esp_reset_reason() is now added as text to data passed to Google Sheets at boot.
- URL-encoding of Google Sheets query parameter payload so that comments containing spaces and other special characters are transmitted correctly. Previously the comment field was appended raw, causing transit to fail.

## [V3.0.1] - 2026-04-18

### Fixed

- `MqttClient.cpp`: MQTT reconnect is now skipped if WiFi is not connected or DHCP has not yet completed (`WiFi.localIP() == 0.0.0.0`). This prevents spurious `rc=-2` (`MQTT_CONNECT_FAILED`) messages on the OLED monitor during WiFi drops or when the ESP32 re-associates before it has a valid IP address.

## [V3.0.0] - 2026-04-18

### Added

- Issue [#6 Add reset functionality](https://github.com/sbv1307/EV-ESP32-energimonitor/issues/6)

```markdown
The reset functionality should have tree way of resetting the device.

1:  A 'soft' reset, which store pulse counts and sub totals to NVS, 
    stop all tasks and call the esp_restart() function to reset the device.
2:  A 'hard' reset, which pulls down a GPIO output pin. External hardware 
    will be connected to the GPIO pin and power cycle the device when 
    triggered by a low output on the GPIO pin.
3:  A 'direct' reset, which store pulse counts and sub totals to NVS to 
    prepare for a power cycle to come.

Regarding item 1 and 2:
These two reset functionalities will be triggered by wirering new logic into the current mqttProcessRxQueue() function. The mqttProcessRxQueue() has a for (JsonPair kv : doc.as()) { ... } section which handles JSON key-value payloads.
Then new logic will handle the JSON key-value { "reset": "option" } where "option" can take the value "soft" or "hard".

Regarding item 3:
This functionality will be triggered by external hardware connected to a GPIO input bin. Then code will have to act extremely fast, as it has to store pulse counts and sub totals to NVS before the power disappear for the device - we are talking milliseconds between a low pulse on the GPIO and the power interruption. SO guidelines about not having to much code in an ISR might be overruled.
```
- Renamed `CHARGING_ANALOG_PIN` to `CHARGING_ANALOG_GPIO` in `config.h`, `ChargingSession.cpp` and `ChargingSession.md` to align with the `_GPIO` suffix convention used by all other GPIO constants.
- Pin layout updated in `config.h`:
  - `PULSE_INPUT_GPIO`: -1 → 33 (interrupt-capable ADC1, INPUT_PULLUP for open-collector energy meter)
  - `HARD_RESET_GPIO`: -1 → 13 (output, drives external power-cycle hardware LOW)
  - `DIRECT_RESET_GPIO`: -1 → 32 (interrupt-capable ADC1, internal pull-up supported — relocated from GPIO 35 which is input-only with no pull-up hardware)
  - `BUTTON_EV_CHARGING_TOGGLE_GPIO`: 25 → 14 (freed GPIO 25 for the price-limit button group)
  - `BUTTON_SMART_CHARGING_TOGGLE_GPIO`: 26 → 25
  - `BUTTON_PRICE_LIMIT_INCREASE_GPIO`: 27 → 26
  - `BUTTON_PRICE_LIMIT_DECREASE_GPIO`: 32 → 27 (freed GPIO 32 by moving it to `DIRECT_RESET_GPIO`)

## [V2.2.0] - 2026-04-16

- Version when change log was introduced.


### Added

- Reset functionality (issue [#6](https://github.com/sbv1307/EV-ESP32-energimonitor/issues/6)):
  - **Soft reset** (`{ "reset": "soft" }` via MQTT): saves pulse counter and subtotal to NVS, then calls `esp_restart()`.
  - **Hard reset** (`{ "reset": "hard" }` via MQTT): saves to NVS, then drives `HARD_RESET_GPIO` LOW to trigger external power-cycle hardware.
  - **Direct reset** (ISR on `DIRECT_RESET_GPIO`, falling edge): a highest-priority FreeRTOS task (`directResetTask`) unblocked from ISR via binary semaphore reads emergency counter copies and calls `saveToNVS()` immediately — designed to complete before power disappears.
- `MQTT_RESET_CMD = "reset"` constant added to `MqttClient.h`.
- `ResetType_t` enum (`RESET_SOFT`, `RESET_HARD`) and `requestReset()` function added to `PulseInputTask`.
- `HARD_RESET_GPIO` and `DIRECT_RESET_GPIO` constants added to `config.h` (default `-1` = disabled).
- Emergency counter mirror (`gEmergencyPulseCounter`, `gEmergencySubtotalPulseCounter`) kept in sync at every counter-change point in `PulseInputTask` for use by the direct-reset path.
- `suspendDirectResetISR()` / `resumeDirectResetISR()` added to `PulseInputTask` and called from OTA `onStart`/`onError` callbacks — same pattern as the pulse ISR.

### Changed

- `DIRECT_RESET_GPIO` reassigned from GPIO 35 to GPIO 32. GPIO 35 is input-only with no internal pull-up hardware (`INPUT_PULLUP` is silently ignored), causing a floating pin that generated spurious interrupts during OTA Wi-Fi activity. GPIO 32 is interrupt-capable, ADC1, and has a functioning internal pull-up.
- Renamed `OledTouchWake::Settings::inputPin` to `inputGpio` and macro `OLED_TOUCH_WAKE_DEFAULT_INPUT_PIN` to `OLED_TOUCH_WAKE_DEFAULT_INPUT_GPIO` to align with the `_GPIO` naming convention used across the codebase.

### Deprecated

### Removed

### Fixed

- **OTA upload stalls / timeout at ~50%**: the `directResetTask` (running at `configMAX_PRIORITIES - 1`) was triggered by spurious FALLING edges on the floating GPIO 35 during OTA, causing concurrent NVS flash writes that competed with the OTA write on the shared SPI flash bus and stalled the ArduinoOTA TCP receive task.  Fixed by:
  1. Adding an `isOtaInProgress()` guard inside `directResetTask` so NVS writes are skipped during OTA.
  2. Detaching the direct-reset interrupt in OTA `onStart` and re-attaching in `onError` (via `suspendDirectResetISR()` / `resumeDirectResetISR()`).
  3. Moving `DIRECT_RESET_GPIO` to GPIO 32 (internal pull-up works) to eliminate the floating-pin root cause.

## [V2.1.3] - 2026-04-14
.
### Added

- 

### Changed

### Deprecated

### Removed

### Fixed

### Security

## [0.0.1] - 2026-04-14

- initial release

<!-- Links -->
[keep a changelog]: https://keepachangelog.com/en/1.0.0/
[semantic versioning]: https://semver.org/spec/v2.0.0.html

<!-- Versions -->
[unreleased]: https://github.com/sbv1307/EV-ESP32-energimonitor/compare/v4.0.0...HEAD
[V4.0.0]: https://github.com/sbv1307/EV-ESP32-energimonitor/compare/v3.2.0...v4.0.0
[V3.2.0]: https://github.com/sbv1307/EV-ESP32-energimonitor/compare/v3.1.0...v3.2.0
[V3.1.0]: https://github.com/sbv1307/EV-ESP32-energimonitor/compare/v3.0.1...v3.1.0
[V3.0.1]: https://github.com/sbv1307/EV-ESP32-energimonitor/compare/v3.0.0...v3.0.1
[V3.0.0]: https://github.com/sbv1307/EV-ESP32-energimonitor/compare/v2.2.0...v3.0.0
[V2.2.0]: https://github.com/sbv1307/EV-ESP32-energimonitor/compare/v2.1.3...v2.2.0
[V2.1.3]: https://github.com/sbv1307/EV-ESP32-energimonitor/compare/v0.0.1...v2.1.3
[0.0.1]: https://github.com/sbv1307/EV-ESP32-energimonitor/releases/tag/v0.0.1

<!-- Releases -->
[V4.0.0-release]: https://github.com/sbv1307/EV-ESP32-energimonitor/releases/tag/v4.0.0
[V3.2.0-release]: https://github.com/sbv1307/EV-ESP32-energimonitor/releases/tag/v3.2.0
[V3.1.0-release]: https://github.com/sbv1307/EV-ESP32-energimonitor/releases/tag/v3.1.0
[V3.0.1-release]: https://github.com/sbv1307/EV-ESP32-energimonitor/releases/tag/v3.0.1
[V3.0.0-release]: https://github.com/sbv1307/EV-ESP32-energimonitor/releases/tag/v3.0.0
[V2.2.0-release]: https://github.com/sbv1307/EV-ESP32-energimonitor/releases/tag/v2.2.0
[V2.1.3-release]: https://github.com/sbv1307/EV-ESP32-energimonitor/releases/tag/v2.1.3
[0.0.1-release]: https://github.com/sbv1307/EV-ESP32-energimonitor/releases/tag/v0.0.1