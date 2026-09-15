# EV ESP32 Energy Monitor

An ESP32-based MQTT interface for a pulse-output energy meter used to monitor
home EV charging. The firmware counts meter pulses, calculates instantaneous
power, detects charging sessions, and publishes energy, cost, Tesla, and device
status data.

## Current Release: V5.2.5

V5.2.5 is the current production firmware baseline. See [changelog.md](changelog.md)
for the release history.

### Features

- Pulse-based total and subtotal energy measurement, with persistent storage in
   ESP32 NVS.
- Analog charging detection using an AC current sensor, with configurable
   threshold, hysteresis, and five-second start/end confirmation.
- Charging-session snapshots that survive reboot or power loss. Completed
   sessions are sent to the `TeslaData` Google Sheet, including energy, battery
   level, range, odometer, location, and Wh/km where available.
- Daily Tesla telemetry to the `TeslaLog` Google Sheet, plus daily, monthly, and
   quarterly cost tracking.
- Email-routed MQTT notifications when `TeslaLog` or `TeslaData` is successfully
   updated, including successful pending-upload retries.
- Home Assistant MQTT discovery for total energy, subtotal energy, power, and
   latest, daily, monthly, and quarterly charging cost.
- Smart-charging state, charging start time, current energy price, three-hour
   low-price reference, and configurable price limit.
- OLED energy display with monitor mode, touch wake, charging state, smart
   charging state, price information, and background updates.
- Separate status and charging LEDs.
- MQTT soft reset and hard reset commands, boot-cause reporting, persistent
   reset diagnostics, and a software fallback if the external hard-reset
   circuit does not power-cycle the board.
- OTA updates through PlatformIO. MQTT, relevant interrupts, OLED updates, and
   persistent pulse/cost state are coordinated during an update.

## MQTT

The device publishes below `ev-e-monitor/<device-name>`, where the device name
is `esp32-doit_<MAC>`.

Home Assistant discovery is published below `homeassistant/`. The main state
payload contains total energy, subtotal energy, power, current price, and the
four cost values. The expected device publications are:

| Topic | Payload | Retained |
| --- | --- | --- |
| `/online` | `True` or `False` connection status | Yes |
| `/sketch_version` | Firmware version, Wi-Fi/MQTT connection details, and boot time | Yes |
| `/state` | JSON state payload described below | Yes |
| `/log` and log subtopics | Timestamped text in the form `YYYY-MM-DD HH:MM:SS - message` | No |
| `/log/email` | Plain text notification without a timestamp, for example `TeslaData updated` or `TeslaLog updated` | No |
| `/err` | Timestamped error text; an empty retained payload clears the error | Yes for errors |

The `/state` JSON payload contains these keys:

```json
{
   "1. Total:": 1234.5,
   "2. Subtotal:": 12.3,
   "3. Forbrug:": 3456.7,
   "currEPrice": 1.25,
   "7. Last Charge:": 4.56,
   "6. Daily Cost:": 7.89,
   "5. Monthly Cost:": 23.45,
   "4. Quarterly Cost:": 67.89
}
```

The device also publishes Home Assistant discovery configuration below
`homeassistant/<component>/<device-name>/<entity>/config`. The discovery
entities represent total energy, subtotal energy, power, and last, daily,
monthly, and quarterly charging cost.

Commands are JSON payloads sent to `/set`. The supported keys are:

```json
{"1. Total:": 1234.5}
{"smartChg": "on"}
{"chgStartTime": "22:00"}
{"currEPrice": 1.25}
{"maxEPrice": 2.10}
{"ePriceLimit": 1.50}
{"reset": "soft"}
{"reset": "hard"}
```

The total-energy command sets the meter reading in kWh. `smartChg` accepts
`on` or `off`; `reset` accepts `soft` or `hard`. Price values use the configured
currency and are normally supplied by the Home Assistant/Tesla integration.

## Physical Controls

The four active-low, debounced pushbuttons use the following GPIOs:

| GPIO | Action |
| --- | --- |
| 14 | Toggle EV charging start/stop |
| 25 | Toggle smart charging |
| 26 | Increase the price limit by 0.10 |
| 27 | Decrease the price limit by 0.10 |

Other default GPIO assignments are:

| GPIO | Function |
| --- | --- |
| 33 | Pulse input from the 74HC14 signal conditioner |
| 34 | AC current sensor for charging detection |
| 13 | External hard-reset output |
| 32 | Direct-reset/power-fail input |
| 2 | Status LED |
| 16 | Charging LED |

Charging detection uses an ADC1 pin because ADC2 cannot be used reliably while
Wi-Fi is active. Tune the analog threshold and hysteresis in
`Firmware/lib/config/config.h` for the installed current sensor.

## Google Sheets and Tesla Authentication

The firmware sends two kinds of telemetry:

- `TeslaLog`: daily data including date/time, battery range, odometer, meter
   reading, latitude, longitude, and cost snapshots.
- `TeslaData`: one row per detected charging session, including start/end
   times, energy used while charging, standby energy when known, battery levels,
   range, odometer, location, and Wh/km.

Tesla token refresh normally uses the local proxy in
`Software/tesla-auth-proxy/`. Deploy it on an always-on Raspberry Pi and set
the following values in the uncommitted
`Firmware/lib/config/privateConfig.h`:

- `TESLA_AUTH_PROXY_URL`, normally `http://<pi-ip>:8787`
- `TESLA_AUTH_PROXY_SHARED_SECRET`, matching the proxy `.env` file

Expose port `8787` to the LAN, not only to `127.0.0.1`, when the ESP32 is on a
different host. See [OTA_SETUP_GUIDE.md](OTA_SETUP_GUIDE.md) and the
[Tesla Auth Proxy README](Software/tesla-auth-proxy/README.md) for deployment
details.

## Build, Upload, and OTA

The firmware is a PlatformIO Arduino project in `Firmware/`. It targets an
`esp32doit-devkit-v1` board and uses the production configuration by default.
Google Sheets integration is enabled by default.

From the `Firmware` directory:

```bash
# USB build
pio run -e esp32doit-devkit-v1

# USB upload
pio run -e esp32doit-devkit-v1 -t upload

# OTA upload using the configured production target
pio run -e esp32doit-devkit-v1_ota -t upload
```

The current OTA environment targets the device at `192.168.11.19` and uses
`3232` on the ESP32 and callback port `8266` on the host. Update the addresses
in `Firmware/platformio.ini` when using another network or the test profile.
The first installation, recovery, or a board with no working OTA firmware must
be uploaded over USB.

## Configuration

Network, MQTT, Tesla, Google Sheets, and secret values are kept in
`Firmware/lib/config/privateConfig.h`, which should not be committed. Start
from `privateConfigExample.h` and select the desired configuration profile in
`Firmware/platformio.ini` (`CONFIG_PROD` or `CONFIG_TEST`).

The firmware does not provide Bluetooth or web-based initial configuration.
Hardware and runtime settings are currently configured in the source and
private configuration files.

## Release Checklist

Before a release, search for `TEST_ONLY_PULSE_ISR_INTERVAL` and remove any
temporary test-only code blocks.

### Tesla Token Refresh Setup (v4.7.0+)

Starting with v4.7.0, Tesla token refresh routes through a proxy service on the Raspberry Pi that handles HTTP/2 + TLS 1.3 to Tesla's auth endpoint (which is required as of July 2026). The ESP32 communicates with the proxy over simple HTTP/1.1 with HMAC-SHA256 request signing.

From v4.8.0, proxy transport selection in firmware is endpoint-aware (`http://` uses non-TLS client, `https://` uses TLS client), and deployment guidance explicitly avoids localhost-only proxy binding for ESP32 access.

**Prerequisites:**
- Raspberry Pi running TeslaMate (or any always-on service)
- Docker and Docker Compose on the Pi

**Setup:**
1. Deploy the proxy from `Software/tesla-auth-proxy/` to `~/tesla-auth-proxy` on your Pi (see [Tesla Auth Proxy README](Software/tesla-auth-proxy/README.md))
2. Update `Firmware/lib/config/privateConfig.h`:
   - Set `TESLA_AUTH_PROXY_URL` to `http://<pi-ip>:8787` (or `http://<pi-hostname>:8787`)
   - Set `TESLA_AUTH_PROXY_SHARED_SECRET` to match the proxy's `.env` `PROXY_SHARED_SECRET`
3. Rebuild and flash the firmware
4. Ensure `Software/tesla-auth-proxy/docker-compose.yml` exposes the proxy for LAN clients (`8787:8787`). Do not use localhost-only binding (`127.0.0.1:8787:8787`) if the ESP32 is on another host.

**Verification:**
- Check Google Sheets for successful telemetry entries (confirms token refresh worked)
- Monitor `docker compose logs` on the Pi for `POST /api/v1/tesla/refresh HTTP/1.1" 200 OK` entries
- Watch MQTT `ev-e-monitor/esp32-doit_*/log/status` for token refresh logs

<sup class="fn">
<span id="f1">[Energy consumption is calculated by measuring the time between pulses provided by the energy meter, connected to the interface](#a1)</span>
</sup>

### Release reminder
- Before release, search for `TEST_ONLY_PULSE_ISR_INTERVAL` and remove any temporary test-only code blocks.


