# EV ESP32 Energy monitor
An ESP32 MQTT interface for an energy meter, used when charging your electrical vehicle at home.

The energy meter connected to the ESP32 MQTT interface, will have a pulse output.

## Release: V5.0.5

This release is the current production firmware baseline for the EV charging monitor.

### Included
- Tesla telemetry and charging-session updates remain wired through the firmware's Google Sheets integration.
- MQTT discovery and cost metrics remain active for Home Assistant monitoring.
- OTA and OLED task stability fixes continue to be included in the current build.

---


### The interface will publish the following to the MQTT broker:
- A configuration, which can be picked up by Home Assistant (HA). HA will then be able to display the data mentioned below.
- Energy meter reading (kWh), which also can be set by the HA integration.
- A calculated energy consumption (W)<sup class="fn"><span id="a1">[1](#f1)</span></sup>
- A daily energy usage (kWh)

### The following will be published to Google Sheets:
- A daily update containing: Date, Time, , Battery range, Odometer, Energy meter reading, Latitude and Longitude
- An update for each charge containing: Date, Start Time, Energy meter reading, KWh used onn Standby, KWh used on charging, Battery level in % at Start, Battery level in % at Stop, range, Odometer, Wh / Km

### An OLED Desplay will show:
- Energy meter reading
- charge start time / No charge plannde
- Smart charging activated
- Charging active.
- Highest price in a 3 hour block having the lowest prices (useful to set electricity price limit)
- Smart Charging Electricity price limit


### Commands 
- stop / start charge
- enable / disable smart charge
- set electricity price limit

### Configuration
- Initial configuration via Bluetooth.<br>**Can not be implemented because bluetooth library is too large.**
- Pushbutton funktion til reset ig initial bluetooth configuration
- Online Configuration WEB link og/eller MQTT?

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


