# EV ESP32 Energy monitor
An ESP32 MQTT interface for an energy meter, used when charging your electrical vehicle at home.

The energy meter connected to the ESP32 MQTT interface, will have a pulse output.


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

<sup class="fn">
<span id="f1">[Energy consumption is calculated by measuring the time between pulses provided by the energy meter, connected to the interface](#a1)</span>
</sup>

### Release reminder
- Before release, search for `TEST_ONLY_PULSE_ISR_INTERVAL` and remove any temporary test-only code blocks.


