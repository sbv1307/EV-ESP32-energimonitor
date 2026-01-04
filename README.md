# EV ESP32 Energy monitor
An ESP32 MQTT interface for an energy meter, used when charging your electrical vehicle at home.

The energy meter monitored, will have a pulse output, which is connected to the interface.


### The interface will publish the following to MQTT:
- A configuration to MQTT, which can be picked up by Home Assistant (HA). HA will then be able to display the data below published to MQTT.
- Energy meter reading, which also can be set by the HA integration.
- A calculated energy consumption<sup class="fn"><span id="a1">[1](#f1)</span></sup>
- A daily energy meter reading to MQTT

### The following will be published to Google Sheets:
- A daily update containing: Date, Time, , Battery range, Odometer, Energy meter reading, Latitude and Longitude
- An update for each charge containing: Date, Start Time, Energy meter reading, KWh used onn Standby, KWh used on charging, Battery level in % at Start, Battery level in % at Stop, range, Odometer, Wh / Km

### An OLED Desplay will show:
- Energy meter reading
- charge start time / Not charge plannde
- price at start time


### Commands 
- stop / start charge
- enable / disable smart charge
- set electricity price limit

### Configuration
- Initial configuration via Bluetooth
- Pushbutton funktion til reset ig initial bluetooth configuration
- Online Configuration WEB link og/eller MQTT?

<sup class="fn">
<span id="f1">[Energy consumption is calculated by measuring the time between pulses provided by the energy meter, connected to the interface](#a1)</span>
</sup>