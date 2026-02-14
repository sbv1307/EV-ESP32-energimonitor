# Charging Session Implementation Summary

## Charging session Solution Design

Extension of the project to measure charging telemetry.
This will be done in two phases:
1: Register 'start charging'
2: Register 'end charging'

This means we will add a new Start/end charging state machine and keep the current day change and resets subtotal functionality.

We will further have to add an analog input trigger.

'start charging' will happen when the level from reading an analog input, extend a configurable setting + a Hysteresis and stays there for 30 seconds (configurable).
'end charging' will happen when the level from reading an analog input, falls below the configurable setting - the Hysteresis and stays there for 30 seconds (configurable).

When charging starts ('start charging'), the following data are store for use when charging ends ('end charging'):
current time (start time), LatestEnergyKwh (start energy kWh), current batteryLevelPercent (start level) and current odometerKm (start odometerKm).
The collected data will be stored in NVS in order to handle outage during charging.

When charging ends ('end charging'), the following data will be send to google sheets, but the sheet for this will be called 'TeslaData':
date, current time (end time), 'start time', LatestEnergyKwh (end energy kWh), ('start energy kWh' - 'last energy kWh' (if known)), ('end energy kWh' - 'start energy kWh'), 'start level', current batteryLevelPercent (end level), rangeKm, current odometerKm (end odometerKm), (('end energy kWh' - 'start energy kWh') / ('end odometerKm' - 'start odometerKm')), latitude and longitude.
The current function sendTeslaTelemetryToGoogleSheets() will be modified to handle the above transmission to 'TeslaData' as well as the current transmission to 'TeslaLog' most of the data are the same. this means that a new google sheet parameter name will be added to privateConfig.h for 'TeslaData'.
The current daily update will stay.
current LatestEnergyKwh (end energy kWh) will then be saved to NVS as 'last energy kWh' for next time charging ends.

Findings first, severity-ordered; include file links with line ranges; keep it concise; end with numbered next steps; no backticks around file paths, links only.

## Github Copilot respond:

## Goal
Implement charging telemetry in two phases while preserving existing daily telemetry and subtotal reset behavior:
1. Register **start charging**
2. Register **end charging**

## What Was Implemented

### 1) Analog-triggered start/end charging state machine
A new charging session module was added:
- `Firmware/lib/tesla/ChargingSession.h`
- `Firmware/lib/tesla/ChargingSession.cpp`

State machine:
- `Idle`
- `StartCandidate`
- `Charging`
- `EndCandidate`

Transition logic:
- Start candidate enters when analog value is `>= threshold + hysteresis`
- Start confirmed if condition is stable for configured start duration
- End candidate enters when analog value is `<= threshold - hysteresis`
- End confirmed if condition is stable for configured end duration

Sampling is periodic (`CHARGING_ANALOG_SAMPLE_INTERVAL_MS`).

### 2) Start snapshot persisted to NVS
On confirmed start, data is captured and persisted:
- `start time` (epoch)
- `start energy kWh` (`LatestEnergyKwh`)
- `start battery level %`
- `start odometer km`

NVS namespace used: `charging`

Stored keys:
- `active`
- `start_epoch`
- `start_energy`
- `start_level`
- `start_odo`
- `last_known`
- `last_end`

This supports outage/reboot recovery during charging sessions.

### 3) End snapshot + TeslaData upload
On confirmed end, end telemetry and energy are collected, then a TeslaData row payload is built and sent.

Payload includes:
- date
- end time
- start time
- end energy kWh
- standby kWh (`start - last_end`, if known)
- charging kWh (`end - start`)
- start battery level %
- end battery level %
- range km
- end odometer km
- Wh/km (`(end-start)*1000 / (endOdo-startOdo)` if distance > 0)
- latitude
- longitude

If upload fails, payload is stored in-memory and retried in loop when possible.

### 4) Google Sheets sender refactor for dual targets
Existing Tesla sheet sender was extended to support both sheets:
- Existing daily telemetry path (`TeslaLog`) remains
- New charging session path (`TeslaData`) added

Changes:
- Added `TeslaSheetTarget` enum (`TeslaLog`, `TeslaData`)
- Added generic sender function: `sendTeslaPayloadToGoogleSheets(...)`
- Kept `sendTeslaTelemetryToGoogleSheets(...)` for daily telemetry compatibility

### 5) Configuration additions
Added charging trigger config constants in:
- `Firmware/lib/config/config.h`

Added TeslaData sheet parameter name in:
- `Firmware/lib/config/privateConfig.h`
- `Firmware/lib/config/privateConfigExample.h`

New constants:
- `CHARGING_ANALOG_PIN`
- `CHARGING_ANALOG_THRESHOLD`
- `CHARGING_ANALOG_HYSTERESIS`
- `CHARGING_START_CONFIRM_SECONDS`
- `CHARGING_END_CONFIRM_SECONDS`
- `CHARGING_ANALOG_SAMPLE_INTERVAL_MS`
- `TESLA_GSHEET_PARAM_NAME_DATA`

### 6) Main loop integration
Integrated charging session handling without removing current daily behavior:
- `initChargingSession()` called from `setup()`
- `handleChargingSession(&networkParams)` called from `loop()`

Existing day-change telemetry + subtotal reset flow is still active.

## Behavior Kept Intact
- Daily telemetry send behavior remains.
- Day-change subtotal reset logic remains.
- Existing pulse counting and MQTT flows remain.

## Validation
- PlatformIO build completed successfully in workspace (`Exit Code: 0`).

## Hardware/Runtime Setup Required
Before field use, set real values for:
- `CHARGING_ANALOG_PIN` (ADC-capable pin)
- threshold/hysteresis and confirm timings
- `TESLA_GSHEET_PARAM_NAME_DATA` (Google Apps Script parameter for TeslaData sheet)

## Notes
- This implementation intentionally keeps charging-session logic separate from the daily telemetry path to reduce regression risk.
- Division-by-zero in Wh/km is guarded by requiring positive odometer delta.
- If no previous end energy exists, standby field is sent empty.
