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

### Release reminder
- Before release, search for `TEST_ONLY_PULSE_ISR_INTERVAL` and remove any temporary test-only code blocks.

### Enhangements

#### Publish energy prise as charge start time:

Asking GithubPilot:
```bash
The home assistant integration 'EV Smart Charging' (can be found here: https://github.com/jonasbkarlsson/ev_smart_charging) offers various informations. For example the attribute current_price for the sensor sensor.ev_smart_charging_charging gives the current price from the electricity price entity. It alsow provide charging_start_time	If charging is planned, the date and time when the charging will start.
I cannot finde a attribute, which provide the electricity price at charging_start_time! Is it possible to implement that in any way?
```
Provided the following answer:
```Bash
Yes—your observation is correct: EV Smart Charging does not expose a built-in “price at charging_start_time” attribute.

You can derive it from raw_two_days + charging_start_time with a template sensor:
```
```bash
template:
  - trigger:
      - platform: state
        entity_id: sensor.ev_smart_charging_charging
        attribute: charging_start_time
      - platform: state
        entity_id: sensor.ev_smart_charging_charging
        attribute: raw_two_days
    sensor:
      - name: EV Smart Charging Price At Start
        unique_id: ev_smart_charging_price_at_start
        unit_of_measurement: "€/kWh"   # adjust to your unit
        state: >
          {% set start = state_attr('sensor.ev_smart_charging_charging', 'charging_start_time') %}
          {% set prices = state_attr('sensor.ev_smart_charging_charging', 'raw_two_days') or [] %}
          {% if not start or prices | length == 0 %}
            {{ none }}
          {% else %}
            {% set start_ts = as_timestamp(start) %}
            {% set ns = namespace(price = none) %}
            {% for p in prices | sort(attribute='start') %}
              {% if as_timestamp(p.start) <= start_ts %}
                {% set ns.price = p.value %}
              {% endif %}
            {% endfor %}
            {{ ns.price if ns.price is not none else none }}
          {% endif %}
        attributes:
          charging_start_time: "{{ state_attr('sensor.ev_smart_charging_charging', 'charging_start_time') }}"
```

- This picks the latest price interval starting at or before planned start.
- Works with 15-min and 60-min data.
- If no schedule exists, sensor becomes unknown/empty.

**How will I add this to Home Assistant?**
Add it as a Template Sensor in Home Assistant, then reload templates.

Fastest way (UI)

- Go to Settings → Devices & Services → Helpers
- Click Create Helper → Template → Template a sensor
- Name: EV Smart Charging Price At Start
- State template: paste this:

```Bash
{% set start = state_attr('sensor.ev_smart_charging_charging', 'charging_start_time') %}
{% set prices = state_attr('sensor.ev_smart_charging_charging', 'raw_two_days') or [] %}
{% if not start or prices | length == 0 %}
  {{ none }}
{% else %}
  {% set start_ts = as_timestamp(start) %}
  {% set ns = namespace(price = none) %}
  {% for p in prices | sort(attribute='start') %}
    {% if as_timestamp(p.start) <= start_ts %}
      {% set ns.price = p.value %}
    {% endif %}
  {% endfor %}
  {{ ns.price if ns.price is not none else none }}
{% endif %}
```
- Unit: set your real unit (for example €/kWh, NOK/kWh, SEK/kWh)
- Save

**Recommended trigger-based YAML version (more reliable updates)**

- In configuration.yaml (or your templates package), add:

```Bash
template:
  - trigger:
      - platform: state
        entity_id: sensor.ev_smart_charging_charging
        attribute: charging_start_time
      - platform: state
        entity_id: sensor.ev_smart_charging_charging
        attribute: raw_two_days
    sensor:
      - name: EV Smart Charging Price At Start
        unique_id: ev_smart_charging_price_at_start
        unit_of_measurement: "€/kWh"
        state: >
          {% set start = state_attr('sensor.ev_smart_charging_charging', 'charging_start_time') %}
          {% set prices = state_attr('sensor.ev_smart_charging_charging', 'raw_two_days') or [] %}
          {% if not start or prices | length == 0 %}
            {{ none }}
          {% else %}
            {% set start_ts = as_timestamp(start) %}
            {% set ns = namespace(price = none) %}
            {% for p in prices | sort(attribute='start') %}
              {% if as_timestamp(p.start) <= start_ts %}
                {% set ns.price = p.value %}
              {% endif %}
            {% endfor %}
            {{ ns.price if ns.price is not none else none }}
          {% endif %}
```
- Then go to Developer Tools → YAML and run Reload Template Entities (or restart HA).

**Check it works**

- Open Developer Tools → States
- Find sensor.ev_smart_charging_price_at_start
- Confirm it gets a value when charging_start_time exists.