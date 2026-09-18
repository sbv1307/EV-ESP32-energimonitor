# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog],
and this project adheres to [Semantic Versioning].

## [V5.3.0] - 2026-09-18

### Changed

- **Sketch version** bumped to `V5.3.0` in `Firmware/lib/config/config.h`.
- **Price-limit buttons now control the TESLA Smart Charging "Low price charging level"** entity instead of the "Electricity price limit" entity (issue #28). `gEnergyPriceLimit` is renamed to `gEnergyLowPriceLimit`, and the button-published MQTT payload key changes from `price_limit` to `low_price_limit`.
- **`gEnergyLowPriceLimit` is now persisted in NVS** (`CONFIG_NVS_NAMESPACE`, key `lowPriceLim`). At boot it is restored from NVS, or initialized to `INITIAL_LOW_PRICE_LIMIT` (0.01) if no value is stored yet.
- **`gEnergyLowPriceLimit` resets to `INITIAL_LOW_PRICE_LIMIT` when a charging session ends**, is persisted to NVS, and the new value is published to `homeassistant/<device-name>/ev-e-monitor/button` as `{"low_price_limit": ...}`.
- **First increase/decrease press after a reset** sets `gEnergyLowPriceLimit` to `gEnergyPriceRef + 0.01` (regardless of which button was pressed), rather than applying the usual +/- 0.10 step.
- **Increase/decrease no longer clamps at zero**: `gEnergyLowPriceLimit` can go negative, since energy prices can be negative.
- **OLED display** now shows `gEnergyLowPriceLimit` instead of `gEnergyPriceLimit`.
- **Removed the inbound `ePriceLimit` `/set` command**: since the buttons now derive `gEnergyLowPriceLimit` from `gEnergyPriceRef` and NVS, the old Home Assistant sync path for this value is no longer needed.

## [V5.2.6] - 2026-09-15

### Changed

- **Sketch version** bumped to `V5.2.6` in `Firmware/lib/config/config.h`.
- **Self-recovery watchdog for MQTT connectivity**: the firmware now monitors MQTT state and escalates WiFi/MQTT recovery after a sustained failure instead of waiting indefinitely for a manual power cycle.
- **Charging-aware reset delay**: a queued reset is now deferred while charging is active and only executes after charging has remained stopped for 60 seconds.

### Fixed

- **MQTT disconnects no longer leave the device stuck offline**: if WiFi is still up but MQTT cannot reconnect, the network layer now attempts a stronger recovery routine and can trigger a safe reset path when the connection remains dead.
- **Reset requests during active charging no longer interrupt an ongoing charge**: `requestReset()` now defers hard/soft reset actions until the charger has been idle for at least 60 seconds, reducing unnecessary interruptions during active charging.

## [V5.2.5] - 2026-09-09

### Changed

- **Sketch version** bumped to `V5.2.5` in `Firmware/lib/config/config.h`.
- **MQTT publication documentation** added to `README.md`, including the
  expected device topics and payload formats.

### Fixed

- **Email-routed MQTT notifications no longer include a timestamp**:
  `publishMqttLogEmail()` now publishes the original message unchanged, such
  as `TeslaData updated` or `TeslaLog updated`. Regular `/log` and `/err`
  messages continue to include their timestamp.

## [V5.2.4] - 2026-09-09

### Changed

- **Sketch version** bumped to `V5.2.4` in `Firmware/lib/config/config.h`.

### Added

- **Email-routed Google Sheets update notifications**: successful `TeslaLog`
  uploads now publish `TeslaLog updated` to `/log/email`, and successful
  `TeslaData` uploads publish `TeslaData updated`. The notifications are also
  emitted when queued or pending uploads succeed on retry.

## [V5.2.3] - 2026-09-08

### Fixed

- **Daily cost reset before Daily Telemetry reached Google Sheets (GitHub issue #26)**: `handleDailyTelemetry()` in `Firmware/src/main.cpp` queued the daily/monthly/quarterly telemetry send via `passTeslaTelemetryToGoogleSheets()`/`sendTeslaTelemetryToGoogleSheets()`, then immediately called `requestDailyCostReset()` (and, on the 1st/quarter-start, `requestMonthlyCostReset()`/`requestQuarterlyCostReset()`). The daily send runs asynchronously in its own task and can take seconds to fetch Tesla telemetry over HTTPS, so by the time it read the live cost via `getLatestCostSnapshot()`, `PulseInputTask` had often already zeroed it - `TeslaLog` in Google Sheets could show `0` for the day's cost. Cost values are now snapshotted in `handleDailyTelemetry()` *before* any reset is requested and passed explicitly through a new `TeslaCostSnapshot` parameter on `sendTeslaTelemetryToGoogleSheets()`/`passTeslaTelemetryToGoogleSheets()` (`Firmware/lib/tesla/TeslaSheets.h`/`.cpp`), removing the internal `getLatestCostSnapshot()` re-read at send time so the reported cost can no longer race the reset. The pending-telemetry retry path (used when WiFi/the send queue is busy) now also carries its own snapshot forward.

## [V5.2.2] - 2026-09-08

### Changed

- **Sketch version** bumped to `V5.2.2` in `Firmware/lib/config/config.h`.

### Added

- **Boot-cause classification extended to OTA and soft resets**: `bootReasonToString()` in `Firmware/src/main.cpp` now also reports `OTA_UPDATE` (reboot triggered by a completed OTA firmware update) and `SOFT_RESET` (a soft reset was requested, e.g. via MQTT), in addition to the existing `HARD_RESET`/`HARD_RESET(SW fallback)`/`DIRECT_RESET` causes. `OtaService.cpp` now writes `BOOT_CAUSE_OTA` before its `esp_restart()` via a new exported `markBootResetCauseForNextBoot()` (`Firmware/lib/pulsInput/PulseInputTask.h/.cpp`), and `PulseInputTask.cpp` writes `BOOT_CAUSE_SOFT` on a soft-reset request.
- **MQTT log entries for reset commands**: incoming MQTT `soft`/`hard` reset commands, and the hard-reset software fallback error, are now also mirrored to the `/log` topic (`publishMqttLog()`), not just the retained `/err` topic, making them visible in the regular log stream (`Firmware/lib/mqtt/MqttClient.cpp`, `Firmware/src/main.cpp`).

### Fixed

- **Retained `/err` clear published a non-empty payload**: `publishMqttLog()` always prepends a timestamp, so the previous "clear" call (`publishMqttError("", true)`) never actually produced a zero-length payload and did not delete the retained message from the broker. New `clearMqttError()` helper (`Firmware/lib/mqtt/MqttClient.cpp/.h`) publishes a true zero-length retained payload instead, and `Firmware/src/main.cpp` now calls it after a successful real hard reset.

## [V5.2.1] - 2026-09-08

### Fixed

- **RESET_HARD could hang forever (GitHub issue #24)**: `PulseInputTask.cpp` drove `HARD_RESET_GPIO` HIGH and then parked forever in `while(true) vTaskDelay(portMAX_DELAY)`, relying on the external power-cycle circuit to physically reset the board. If that circuit does not respond, the task now falls back to `esp_restart()` after a grace period (`HARD_RESET_FALLBACK_TIMEOUT_MS`, default 15 s, in `Firmware/lib/config/config.h`), so a RESET_HARD request can never stall the device again.

### Added

- **Boot-cause classification in boot telemetry**: the "Boot reason: ..." message sent to Google Sheets (and the boot-diagnostics MQTT log) now distinguishes `HARD_RESET` (a hard reset was requested and the board was actually power-cycled), `HARD_RESET(SW fallback)` (a hard reset was requested but the power-cycle hardware did not respond within the grace period, so the software fallback fired), and `DIRECT_RESET` (the direct-reset/power-fail path saved state before power was lost - e.g. external kill switch or outage - without a hard reset being requested) from the raw ESP reset reasons (`POWERON` = unexpected power on, `SW`, `PANIC`, ...). Implemented via a new NVS `reset_cause` key (`BOOT_CAUSE_*` in `Firmware/lib/globals/globals.h`) written by `PulseInputTask.cpp` before the reset/power loss and read-and-cleared at boot in `initializeGlobals()`; a requested hard reset takes precedence over the direct-reset observation of the same power cut.
- **MQTT error topic `ev-e-monitor/<device-mac>/err`**: when the hard-reset software fallback fired (power-cycle circuit did not respond), an error is published once to the retained `/err` topic at the next boot, as soon as WiFi is connected. (Published at next boot rather than when the fallback fires, because the queued MQTT publish would not drain before the fallback's `esp_restart()`.) The retained error auto-clears: after a *successful* real hard reset (HARD marker + POWERON reason, i.e. the circuit responded), an empty retained message is published to `/err`, deleting the stale error from the broker. New `publishMqttError()` helper in `Firmware/lib/mqtt/MqttClient.cpp`.
- **Intentional reboots mark the next boot as controlled**: the `requestReset()` handling (soft and hard) and the OTA `onStart()` path now write `controlled_pwr=true` before rebooting, so the uncontrolled-boot safety net no longer fires a follow-up RESET_HARD after MQTT-commanded resets, OTA updates, or the new software fallback itself (which would otherwise have caused a repeating 10-minute reset loop while the power-cycle hardware is unresponsive).

## [V5.2.0] - 2026-09-06

### Changed

- **Sketch version** bumped to `V5.2.0` in `Firmware/lib/config/config.h`.
- **OLED touch diagnostics** disabled (`OLED_TOUCH_DIAGNOSTICS_MQTT_ENABLED = false`) after the touch-wake investigation concluded.

### Fixed

- **Touch wake stopped working after hours of uptime**: `Firmware/lib/oled_energy_display/oled_touch_wake.cpp` previously adapted the touch baseline *only while touched*, so every short noise dip (~100-150 ms, a few samples) permanently ratcheted the baseline down (observed 31 -> 27 -> 25 over ~2 h) until the threshold fell below what a real finger touch can reach. The baseline now tracks **untouched** readings only, via a slow bidirectional fixed-point EMA (1/128 per sample), so it follows environmental drift and self-recovers after noise; a continuous >60 s "touch" triggers re-calibration (covers the opposite lockout), and the default debounce was raised from 2 to 4 samples (200 ms) to reject the observed noise bursts before they can fire spurious wakes.

### Remarks on the 2026-09-02 investigation

- The "pulse count stops, works after reboot" root cause was confirmed as the uncontrolled-boot safety net firing `RESET_HARD` ~10 min after boot while the only `controlled_pwr` writer (directResetTask) was disabled: PulseInputTask drove `HARD_RESET_GPIO` and parked forever in `vTaskDelay(portMAX_DELAY)` because the physical power-cycle circuit did not reset the board at the time.
- The Q1/Direct Reset defect was a **grounding-topology issue, not radiated EMI**: Q1's base was referenced to supply GND while its emitter sat on the ESP32 GND-by-Vin; ground bounce between the two GND paths pulled the emitter below the base and held Q1 on (matching the scope reading of GPIO32 solid LOW - i.e. the GPIO32 investigation *disproved* spurious edges rather than proving EMI).
- **Still open**: if the hard-reset hardware fails to power-cycle, the firmware has no software fallback (task parks forever). Proposed hardening: `esp_restart()` fallback after a ~15 s grace period (NVS is already saved by then), plus an optional fast-path that fires the hard reset as soon as MQTT connects after an uncontrolled boot instead of waiting 10 min. Strategy not yet decided.

## [V5.1.6] - 2026-09-04

### Changed

- **Sketch version** bumped to `V5.1.6` in `Firmware/lib/config/config.h`.
- **Direct Reset functionality** re-enabled in `Firmware/lib/pulsInput/PulseInputTask.cpp` after the hardware fix, restoring emergency NVS persistence for pulse and cost data on the Direct Reset signal.
- **Uncontrolled-boot hard-reset fallback** enabled via `UNCONTROLLED_BOOT_HARD_RESET_ENABLED` in `Firmware/lib/config/config.h`; boots preceded by a Direct Reset are recognized as controlled, while boots without the controlled-power-cycle marker are treated as possible power outages after the configured delay.

## [V5.1.4] - 2026-09-04

### Changed

- **Sketch version** bumped to `V5.1.4` in `Firmware/lib/config/config.h`.
- **Task stack watermark monitoring** added for LED, direct-reset, OLED, and related tasks in `Firmware/lib/globals/globals.h`/`.cpp`, with larger task stack sizes configured to match observed runtime usage.
- **Pulse input task diagnostics** expanded in `Firmware/lib/pulsInput/PulseInputTask.cpp` with stage tracking and counters to make pulse-count stalls easier to isolate during debugging.

### Fixed

- **Pulse-count stall / data loss recovery**: `PulseInputTask.cpp` now keeps a more resilient emergency snapshot of pulse and cost totals, drains pending counter updates more defensively, and preserves state across restarts and reset-related edge cases.
- **Direct-reset path safety**: the direct-reset emergency save path remains disabled until the hardware issue is resolved, preventing the code from triggering a false reset loop while still keeping the emergency NVS snapshot logic available for safe re-enable.
- **Display responsiveness**: `Firmware/src/main.cpp` and `Firmware/lib/oled_energy_display/oled_touch_wake.cpp` now avoid long scheduling delays that could defer OLED wake/refresh handling behind WiFi stack checks.

## [V5.1.0] - 2026-08-30

### Changed

- **Sketch version** bumped to `V5.1.0` in `Firmware/lib/config/config.h`.
- **LED status/charging indicators split into two independent LEDs**: `Firmware/lib/led/LedTask.cpp`/`.h` now support two independently driven LEDs via `enum class LedId { Status, Charge }` and `sendLedCommand(LedId, const char*)`, replacing the single shared `LED_BUILTIN` task.
- **LED GPIO assignments** added to `config.h`: `LED_STATUS_GPIO` (2, boot/WiFi/MQTT connectivity status) and `LED_CHARGE_GPIO` (16, charging state + per-pulse activity blip).
- **main.cpp** boot/WiFi/MQTT status LED commands now target `LedId::Status`.
- **ChargingSession.cpp** charging-state LED commands now target `LedId::Charge`, and no longer require `gMqttConnected` to be true (that gate was only needed while the LED was shared with the connectivity indicator).
- **PulseInputTask.cpp** per-pulse LED blink now targets `LedId::Charge`, blipping against the charge LED's current steady state.

## [V5.0.6] - 2026-08-30

### Changed

- **Sketch version** bumped to `V5.0.6` in `Firmware/lib/config/config.h`.
- **Pulse input interrupt mode** changed from `FALLING` to `RISING` edge detection in `Firmware/lib/config/config.h` to match the inverted output of the 74HC14 Schmitt trigger signal conditioning circuit.
- **Pulse input GPIO configuration** now uses plain `INPUT` mode (no internal pull-up/down) since the 74HC14 output is push-pull and hardware provides 4.7kΩ external pull-up and 100nF filtering.
- **PulseInputTask.cpp**: `attachPulseInputInterrupt()` now accepts `pinInputMode` parameter instead of auto-selecting based on edge mode, allowing hardware-specific pin configuration.
- **Hardware signal chain documentation**: improved comments in `config.h` explaining the complete 74HC14 inverting Schmitt trigger circuit (U2A with R17 pull-up and C18 filter).

### Updated

- **platformio.ini**: OTA upload configuration network addresses updated to current test environment (changed from 192.168.22.x to 192.168.11.x).

## [V5.0.5] - 2026-08-29

### Changed

- **Release**: V5.0.5 is marked as the current released firmware baseline.
- **Sketch version**: kept at `V5.0.5` in `Firmware/lib/config/config.h`.

## [Unreleased]

### Fixed

- **OLED energy display update lag**: `main.cpp`'s `calculateNextDelayMs()` could let `loop()` sleep up to ~5s (from the WiFi-check/stack-log schedule) before re-checking `gDisplayUpdateAvailable`, delaying the OLED refresh well after a pulse's LED blink. Capped the loop's sleep at 200ms so pending display updates and the MQTT RX queue are polled promptly.

### Added

- **Stack high-water tracking for previously unmonitored tasks**: added `LED_TASK_STACK_SIZE`, `DIRECT_RESET_TASK_STACK_SIZE`, and `OLED_UPDATE_TASK_STACK_SIZE` constants plus matching watermark globals (`gLedStatusTaskStackHighWater`, `gLedChargeTaskStackHighWater`, `gDirectResetTaskStackHighWater`, `gOledUpdateTaskStackHighWater`) in `globals.h`/`.cpp`. `LedTask.cpp`, `PulseInputTask.cpp` (`direct_rst` task), and `oled_library.cpp` now record `uxTaskGetStackHighWaterMark()`, and `main.cpp`'s periodic `STACK_WATERMARK` block logs suggested size changes for all three via `log/stack/ledStatus`, `log/stack/ledCharge`, `log/stack/directReset`, and `log/stack/oledUpdate`.

## [V5.0.4] - 2026-08-23

### Changed

- **Tesla telemetry task stack size** increased from `8750` to `8938` words.

## [V5.0.3] - 2026-08-06

### Changed

- **Sketch version** bumped to `V5.0.3` in `Firmware/lib/config/config.h`.
- **OLED background updater shutdown** in `Firmware/lib/oled_energy_display/oled_library.cpp` now uses cooperative stop instead of force-deleting the task.

### Fixed

- **OTA upload stall at start**: resolved a potential mutex deadlock during OTA `onStart` when stopping OLED background updates; this could block OTA handling right after upload initialization.

## [V5.0.2] - 2026-08-05

### Added

- **MQTT cost sensors for Home Assistant**: added retained cost metrics for latest charge, daily total, monthly total, and quarterly total so HA can display charging cost alongside energy usage.
- **Cost entity discovery**: MQTT discovery publishes separate Home Assistant entities for latest charge, daily, monthly, and quarterly costs.

### Changed

- **Sketch version** bumped to `V5.0.2` in `Firmware/lib/config/config.h`.
- **Readable HA labels for costs**: discovery payload uses human-readable labels for cost entities while preserving stable JSON keys.
- **OLED MQTT failure lines shortened** in `Firmware/lib/mqtt/MqttClient.cpp` so the most relevant state/broker text fits the monitor line width.

### Fixed

- **Home Assistant cost entity visibility**: discovery for cost entities now uses unique config topics so entities no longer overwrite each other.
- **MQTT discovery topic truncation**: increased MQTT topic buffer size in `Firmware/lib/mqtt/MqttMessage.h` and added publish length checks in `Firmware/lib/mqtt/MqttClient.cpp` to prevent silent truncation for long discovery topics.

### Validation

- Confirmed stable MQTT reconnect and payload publishing after the V5.0.2 changes.
- Confirmed all expected MQTT-discovered Home Assistant entities are visible and updating.

## [V5.0.1] - 2026-08-05

### Changed

- **Sketch version** bumped to `V5.0.1` in `Firmware/lib/config/config.h`.
- **MQTT discovery compatibility**: Home Assistant entity names can now include spaces and display punctuation without breaking templates.

### Fixed

- **Discovery template key handling** in `Firmware/lib/mqtt/MqttClient.cpp`: `value_template` and `command_template` now escape entity names correctly.
- **Discovery identity/topic safety** in `Firmware/lib/mqtt/MqttClient.cpp`: `unique_id` and discovery config topic entity segment now use a normalized token, avoiding invalid characters from display names.

## [V4.8.0] - 2026-07-21

### Fixed

- **Tesla proxy connection refused path**: corrected malformed proxy URL in `Firmware/lib/config/privateConfig.h` so `TESLA_AUTH_PROXY_URL` resolves to a valid endpoint.
- **Proxy LAN reachability**: updated `Software/tesla-auth-proxy/docker-compose.yml` port mapping from localhost-only binding to LAN-reachable binding so ESP32 can connect to the proxy from the network.
- **Proxy transport selection in firmware**: `Firmware/lib/tesla/TeslaApi.cpp` now selects `WiFiClient` for `http://` proxy endpoints and `WiFiClientSecure` for `https://` endpoints.

### Changed

- **Sketch version** bumped to `V4.8.0` in `Firmware/lib/config/config.h`.

### Validation

- Confirmed proxy health endpoint is reachable over LAN (`/healthz`) and telemetry refresh no longer fails with `connection refused` in the resolved setup.

## [V4.7.0] - 2026-07-20

### Added

- **Tesla auth proxy integration**: Token refresh now routes through a local proxy service (on Raspberry Pi) that handles HTTP/2 + TLS 1.3 transport to Tesla auth endpoint. Configured via `TESLA_AUTH_PROXY_URL` and `TESLA_AUTH_PROXY_SHARED_SECRET` in `privateConfig.h`.
- **HMAC-SHA256 request signing**: Token refresh requests are signed with HMAC-SHA256 (base64url) to prevent relay attacks. Signature includes device ID, timestamp, nonce, and request body.
- **Proxy fallback behavior**: If `TESLA_AUTH_PROXY_URL` is empty, firmware falls back to direct Tesla call (legacy path for backward compatibility).
- **New MQTT log messages**: Token refresh via proxy is logged to MQTT status topic so proxy health is visible in the device monitoring.

### Changed

- **Token refresh method** now uses `teslaRefreshViaProxy()` when proxy is configured, instead of direct Tesla endpoint call.
- **Reduced firmware TLS/HTTP/2 complexity**: HTTP/2 negotiation now happens on the proxy (always-on Pi service) rather than on ESP32, simplifying firmware and reducing runtime TLS stack load.

### Suggested Follow-ups

- Monitor MQTT log messages for proxy errors during the first week of operation.
- If proxy goes down, the device will retry token refresh at the next scheduled interval (typically ~24 hours).
- The temporary debug logs for token cadence can be removed after confirming refresh behavior is stable.
- Replace `client.setInsecure()` with proper root CA certificates for both proxy and Tesla vehicle API calls.

## [V4.5.1] - 2026-07-19

### Added

- **Tesla auth transport update**: `TeslaApi.cpp` now Tesla-api-has-changed so the auth flow can negotiate HTTP/2 where the TLS stack supports it.
- **Clearer auth failure hint**: Tesla auth POST failures now append a transport hint that mentions HTTP/2, TLS 1.3, and ALPN support expectations.
- **Tesla telemetry failure logging**: `TeslaSheets.cpp` now publishes MQTT status logs when Tesla telemetry fetches fail, making runtime auth/telemetry problems visible without relying on serial output.

### Changed

- **Sketch version** bumped to `V4.5.1` in `Firmware/lib/config/config.h`.
- **Tesla auth client setup** was centralized so the auth path consistently applies the same TLS settings before token refresh requests.

### Suggested Follow-ups

- Replace `setInsecure()` with a proper root CA for Tesla auth and Tesla vehicle requests.
- Add a runtime log of the negotiated TLS/ALPN result if the Arduino-ESP32 core exposes it cleanly.
- Add a short smoke-test procedure for live Tesla auth refresh so future API changes can be validated faster.

## [V4.4.1] - 2026-06-11

### Added

- **Hardware RC delay filter**: Added 100nF ceramic capacitor between NPN transistor base and GND to prevent spurious power-cycle triggers during ESP32 boot (time constant ≈1 ms provides immunity to startup transients while allowing normal power-cycle commands).

### Changed

- **Hard reset** (`{ "reset": "hard" }` via MQTT): saves to NVS, then drives `HARD_RESET_GPIO` HIGH to trigger external power-cycle hardware.
- **Pin layout** updated in `config.h`:
  - `HARD_RESET_GPIO`: -1 → 13 (output, active-HIGH to trigger power-cycle).
- **GPIO13 initialization** moved to earliest boot point in `Firmware/src/main.cpp` `setup()` (immediately after LED init, before tasks and splash screen) via new `initResetGpioPins()` function to prevent floating-state glitches.
- **GPIO13 initialization sequence** changed to glitch-safe: preload output to LOW via `digitalWrite()` before switching `pinMode()` to OUTPUT, preventing spurious pulses from uncontrolled boot state.
- **Hardware resistor change** in power-cycle trigger circuit: transistor base pulldown changed from 100 kΩ to 10 kΩ to improve bias stability during startup and with the added RC filter cap.

### Fixed

- Fixed spurious power-cycle triggers on boot: GPIO13 was uncontrolled from power-on until task startup, allowing capacitive coupling and noise to drive the transistor base and trigger power-cycle logic. Now mitigated by early safe-state initialization + RC delay + stronger pulldown.
- Fixed direct-reset emergency save initialization in `Firmware/lib/pulsInput/PulseInputTask.cpp` so `pulse_count`/`subtotal_count` are seeded from NVS before direct-reset ISR is enabled, preventing a startup race that could persist `pulse_count=0`.

## [V4.4.0] - 2026-06-09

### Added

- Added uncontrolled-boot detection flow using NVS key `controlled_pwr`:
  - In `Firmware/lib/pulsInput/PulseInputTask.cpp`, direct-reset handling now writes `controlled_pwr=true` to NVS immediately after storing `pulse_count` and `subtotal_count`.
  - In `Firmware/lib/globals/globals.cpp`, `initializeGlobals()` now loads `controlled_pwr` (default `false`) into runtime global `gControlledPowerCycle`, then immediately writes `controlled_pwr=false` back to NVS.
- Added configurable delayed hard-reset fallback in `Firmware/lib/config/config.h` via `UNCONTROLLED_BOOT_HARD_RESET_DELAY_MINUTES` (default `10`).
- Updated boot monitor message in `Firmware/src/main.cpp` setup:
  - `Ctrl Boot OK` when `gControlledPowerCycle` is `true`.
  - `Un-ctrl Boot OK` when `gControlledPowerCycle` is `false`.
- Added loop-time uncontrolled-boot action in `Firmware/src/main.cpp`:
  - After configured delay and while `gControlledPowerCycle==false`, the firmware now shows `Un-ctrl boot->hard reset`, publishes MQTT log `Uncontrolled boot detected, requesting RESET_HARD`, and calls `requestReset(RESET_HARD)`.

## [V4.3.0] - 2026-06-07

### Changed

- Updated direct-reset interrupt edge in `Firmware/lib/pulsInput/PulseInputTask.cpp` from `FALLING` to `RISING` for both startup and ISR re-attach paths (`startDirectResetISR()` and `resumeDirectResetISR()`).
- Kept `DIRECT_RESET_GPIO` configured as `INPUT_PULLUP` for open-collector compatibility, so the input stays biased when the transistor is off.

## [V4.2.4] - 2026-05-15

### Fixed

- Fixed MQTT `/set` handling for `smartChg` so JSON boolean payloads like `{ "smartChg": true }` and `{ "smartChg": false }` now update `gSmartChargingActivated` correctly and refresh the OLED `Smart ON/OFF` header.
- Extended boolean command parsing in `Firmware/lib/mqtt/MqttClient.cpp` to accept native JSON booleans in addition to text values such as `"true"` and `"false"`, preventing `smartChg` from being interpreted as `false` when Home Assistant sends a boolean value.
- Stabilized pulse input interrupt configuration in `Firmware/lib/pulsInput/PulseInputTask.cpp` by selecting `INPUT_PULLUP` for `FALLING`/`LOW` triggers and `INPUT_PULLDOWN` for `RISING`/`HIGH` triggers, so the GPIO idle state matches the interrupt mode.

## [V4.2.3] - 2026-05-14

### Changed

- `PULSE_INPUT_TASK_STACK_SIZE` changed from `2525` to `2642` words based on payload from MQTT topic `PULSE_INPUT_TASK_STACK_SIZE`.

## [V4.2.2] - 2026-05-14

### Changed

- Ignored and de-tracked generated hardware history artifacts by removing tracked file `Hardware/.history`.

## [V4.2.1] - 2026-05-14

### Added

- Added optional headless charging-debug instrumentation in `Firmware/lib/tesla/ChargingSession.cpp` to mirror significant analog level and `ChargingState` transitions on OLED when serial logging is unavailable.
- Added KiCad library submodule `Hardware/library` (source: `https://github.com/sbv1307/kicad-library.git`) to keep hardware symbols/footprints versioned with the project.

### Changed

- Updated MQTT topic prefix in `Firmware/lib/mqtt/MqttClient.h` from `ev-e-charging/` to `ev-e-monitor/`.

## [V4.2.0] - 2026-04-26

### Added

- Issue [#12 Change from DC level to AC value for setting ChargingState](https://github.com/sbv1307/EV-ESP32-energimonitor/issues/12)

### Changed

- Replaced the single DC-level `analogRead(CHARGING_ANALOG_GPIO)` charging trigger with AC RMS sampling via `readAcRms()` in `Firmware/lib/tesla/ChargingSession.cpp`, allowing `ChargingState` to be derived from the SCT01-T10/50A AC sensor signal.
- Updated the charging sensor configuration in `Firmware/lib/config/config.h` for SCT01-T10/50A operation and removed the fixed `CHARGING_AC_ADC_BIAS` constant in favor of per-sample-window DC-offset removal.
- Tuned default charging-detection constants in `Firmware/lib/config/config.h` for SCT01-T10/50A to better match charging start around 900 W: `CHARGING_ANALOG_THRESHOLD=90` and `CHARGING_ANALOG_HYSTERESIS=12`.

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
[unreleased]: https://github.com/sbv1307/EV-ESP32-energimonitor/compare/v4.8.0...HEAD
[V4.8.0]: https://github.com/sbv1307/EV-ESP32-energimonitor/compare/v4.7.0...v4.8.0
[V4.7.0]: https://github.com/sbv1307/EV-ESP32-energimonitor/compare/v4.5.1...v4.7.0
[V4.5.1]: https://github.com/sbv1307/EV-ESP32-energimonitor/compare/v4.4.1...v4.5.1
[V4.4.1]: https://github.com/sbv1307/EV-ESP32-energimonitor/compare/v4.4.0...v4.4.1
[V4.4.0]: https://github.com/sbv1307/EV-ESP32-energimonitor/compare/v4.3.0...v4.4.0
[V4.3.0]: https://github.com/sbv1307/EV-ESP32-energimonitor/compare/v4.2.4...v4.3.0
[V4.2.4]: https://github.com/sbv1307/EV-ESP32-energimonitor/compare/v4.2.3...v4.2.4
[V4.2.3]: https://github.com/sbv1307/EV-ESP32-energimonitor/compare/v4.2.2...v4.2.3
[V4.2.2]: https://github.com/sbv1307/EV-ESP32-energimonitor/compare/v4.2.1...v4.2.2
[V4.2.1]: https://github.com/sbv1307/EV-ESP32-energimonitor/compare/v4.2.0...v4.2.1
[V4.2.0]: https://github.com/sbv1307/EV-ESP32-energimonitor/compare/v4.0.0...v4.2.0
[V4.1.0]: https://github.com/sbv1307/EV-ESP32-energimonitor/compare/v4.0.0...v4.1.0
[V4.0.0]: https://github.com/sbv1307/EV-ESP32-energimonitor/compare/v3.2.0...v4.0.0
[V3.2.0]: https://github.com/sbv1307/EV-ESP32-energimonitor/compare/v3.1.0...v3.2.0
[V3.1.0]: https://github.com/sbv1307/EV-ESP32-energimonitor/compare/v3.0.1...v3.1.0
[V3.0.1]: https://github.com/sbv1307/EV-ESP32-energimonitor/compare/v3.0.0...v3.0.1
[V3.0.0]: https://github.com/sbv1307/EV-ESP32-energimonitor/compare/v2.2.0...v3.0.0
[V2.2.0]: https://github.com/sbv1307/EV-ESP32-energimonitor/compare/v2.1.3...v2.2.0
[V2.1.3]: https://github.com/sbv1307/EV-ESP32-energimonitor/compare/v0.0.1...v2.1.3
[0.0.1]: https://github.com/sbv1307/EV-ESP32-energimonitor/releases/tag/v0.0.1

<!-- Releases -->
[V4.8.0-release]: https://github.com/sbv1307/EV-ESP32-energimonitor/releases/tag/v4.8.0
[V4.7.0-release]: https://github.com/sbv1307/EV-ESP32-energimonitor/releases/tag/v4.7.0
[V4.4.1-release]: https://github.com/sbv1307/EV-ESP32-energimonitor/releases/tag/v4.4.1
[V4.5.1-release]: https://github.com/sbv1307/EV-ESP32-energimonitor/releases/tag/v4.5.1
[V4.4.0-release]: https://github.com/sbv1307/EV-ESP32-energimonitor/releases/tag/v4.4.0
[V4.3.0-release]: https://github.com/sbv1307/EV-ESP32-energimonitor/releases/tag/v4.3.0
[V4.2.4-release]: https://github.com/sbv1307/EV-ESP32-energimonitor/releases/tag/v4.2.4
[V4.2.3-release]: https://github.com/sbv1307/EV-ESP32-energimonitor/releases/tag/v4.2.3
[V4.2.2-release]: https://github.com/sbv1307/EV-ESP32-energimonitor/releases/tag/v4.2.2
[V4.2.1-release]: https://github.com/sbv1307/EV-ESP32-energimonitor/releases/tag/v4.2.1
[V4.2.0-release]: https://github.com/sbv1307/EV-ESP32-energimonitor/releases/tag/v4.2.0
[V4.1.0-release]: https://github.com/sbv1307/EV-ESP32-energimonitor/releases/tag/v4.1.0
[V4.0.0-release]: https://github.com/sbv1307/EV-ESP32-energimonitor/releases/tag/v4.0.0
[V3.2.0-release]: https://github.com/sbv1307/EV-ESP32-energimonitor/releases/tag/v3.2.0
[V3.1.0-release]: https://github.com/sbv1307/EV-ESP32-energimonitor/releases/tag/v3.1.0
[V3.0.1-release]: https://github.com/sbv1307/EV-ESP32-energimonitor/releases/tag/v3.0.1
[V3.0.0-release]: https://github.com/sbv1307/EV-ESP32-energimonitor/releases/tag/v3.0.0
[V2.2.0-release]: https://github.com/sbv1307/EV-ESP32-energimonitor/releases/tag/v2.2.0
[V2.1.3-release]: https://github.com/sbv1307/EV-ESP32-energimonitor/releases/tag/v2.1.3
[0.0.1-release]: https://github.com/sbv1307/EV-ESP32-energimonitor/releases/tag/v0.0.1