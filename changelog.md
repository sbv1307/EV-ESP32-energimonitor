cl-# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog],
and this project adheres to [Semantic Versioning].

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
  - `DIRECT_RESET_GPIO`: -1 → 35 (input-only sensor pin, interrupt on power-fail signal)
  - `BUTTON_EV_CHARGING_TOGGLE_GPIO`: 25 → 14 (freed GPIO 25 for the price-limit button group)
  - `BUTTON_SMART_CHARGING_TOGGLE_GPIO`: 26 → 25
  - `BUTTON_PRICE_LIMIT_INCREASE_GPIO`: 27 → 26
  - `BUTTON_PRICE_LIMIT_DECREASE_GPIO`: 32 → 27 (freed GPIO 32 as available ADC1 pin)

## [Unreleased]

- Version when change log was introduced.

## [V2.2.0] - 2026-04-16

### Added

- Reset functionality (issue [#6](https://github.com/sbv1307/EV-ESP32-energimonitor/issues/6)):
  - **Soft reset** (`{ "reset": "soft" }` via MQTT): saves pulse counter and subtotal to NVS, then calls `esp_restart()`.
  - **Hard reset** (`{ "reset": "hard" }` via MQTT): saves to NVS, then drives `HARD_RESET_GPIO` LOW to trigger external power-cycle hardware.
  - **Direct reset** (ISR on `DIRECT_RESET_GPIO`, falling edge): a highest-priority FreeRTOS task (`directResetTask`) unblocked from ISR via binary semaphore reads emergency counter copies and calls `saveToNVS()` immediately — designed to complete before power disappears.
- `MQTT_RESET_CMD = "reset"` constant added to `MqttClient.h`.
- `ResetType_t` enum (`RESET_SOFT`, `RESET_HARD`) and `requestReset()` function added to `PulseInputTask`.
- `HARD_RESET_GPIO` and `DIRECT_RESET_GPIO` constants added to `config.h` (default `-1` = disabled).
- Emergency counter mirror (`gEmergencyPulseCounter`, `gEmergencySubtotalPulseCounter`) kept in sync at every counter-change point in `PulseInputTask` for use by the direct-reset path.

### Changed

### Deprecated

### Removed

### Fixed

### Security

## [V2.1.3] - 2026-04-14

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
[unreleased]: https://github.com/sbv1307/EV-ESP32-energimonitor/compare/v0.0.2...HEAD
[V2.1.3]: https://github.com/sbv1307/EV-ESP32-energimonitor/compare/v0.0.1...v0.0.2
[0.0.1]: https://github.com/sbv1307/EV-ESP32-energimonitor/releases/tag/v0.0.1