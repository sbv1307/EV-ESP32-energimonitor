pleae add two more items to the Home Assistant automations listed at the end.
The enitities comes from the 'EV Smart Charging' integration for Home Assistand. The integration can be found here: https://github.com/jonasbkarlsson/ev_smart_charging/ 

Item one: The Configuration entity number.ev_smart_charging_electricity_price_limit
Item two: Then attribute current_price for entity sensor.ev_smart_charging_charging

The two items will be handled seperatly as number.ev_smart_charging_electricity_price_limit are changed manually very rarely where current_price is chaged every 15 minutes.

Both items will be published to MQTT topic: ev-e-charging/{{ charger_id }}/set where the payload will be a json doc key - value format.
The key for current_price will be currEPrice and the key for number.ev_smart_charging_electricity_price_limit will be ePriceAtStart.
The values for both items are to be published with two numbers in the integer part and two numbers in the decimal part.

The triggers for both items will be:
- When home assistant restarts
- when an item change value
- when ev-e-charging/{{ charger_id }}/online becomes True
- device name changes (input_text.ev_charger_id state changes)

What will 
Current automations:

- id: ev_e_charging_sync
  alias: EV E Charging Sync (Direct MQTT)

  variables:
    charger_id: "{{ states('input_text.ev_charger_id') }}"

  trigger:

    # State changes
    - platform: state
      entity_id: switch.ev_smart_charging_smart_charging_activated

    # ESP comes online
    - platform: mqtt
      topic: ev-e-charging/{{ charger_id }}/online
      payload: "True"

    # HA restart
    - platform: homeassistant
      event: start

    # device name changes
    - platform: state
      entity_id: input_text.ev_charger_id

  action:

    - service: mqtt.publish
      data:
        topic: ev-e-charging/{{ charger_id }}/set
        payload: >
          {"smartChg": {{ is_state('switch.ev_smart_charging_smart_charging_activated','on') | lower }} }
        retain: true

- id: ev_charging_data_publish
  alias: Publish EV charging data to ESP32

  mode: restart

  trigger:

    # Attribute changes
    - platform: state
      entity_id: sensor.ev_smart_charging_charging
      attribute: Charging start time
      id: start_time_changed

    - platform: state
      entity_id: sensor.ev_smart_charging_charging
      attribute: current_price
      id: price_changed

    # ESP comes online
    - platform: mqtt
      topic: ev-e-charging/{{ charger_id }}/online
      payload: "True"

    # HA restart
    - platform: homeassistant
      event: start

    # device name changes
    - platform: state
      entity_id: input_text.ev_charger_id

  action:
    - service: mqtt.publish
      data:
        topic: ev-e-charging/{{ charger_id }}/set
        retain: true
        payload: >
          {% set start = state_attr('sensor.ev_smart_charging_charging', 'Charging start time') %}
          {% set price = state_attr('sensor.ev_smart_charging_charging', 'current_price') %}
          {
            "chg_start_time": "{{ as_timestamp(start) | timestamp_custom('%H:%M') if start else '' }}",
            "current_price": {{ price if price is not none else 0 }}
          }


Pushbotton automations

In Home Assistant automation I need automations to handle the following MQTT payloads which is in JSON format:
- {"ev_charging":"stop"}
- {"ev_charging":"start"}
- {"smart_charging_activated":"off"}
- {"smart_charging_activated":"on"}
- {"price_limit": n.nnn}

The automations are to be added into the existng automations file C:\Users\Steen\Documents\4-Projects\EV-ESP32-energimonitor\Software\homeAssistant\config\automations_charging_monitor.yaml.

This automations .yaml file opertes with two globals: 'charging_monitor_mqtt_prefix' and 'charging_monitor_id'.

The MQTT topic for the payloads is homeassistant/<charging_monitor_id>/<charging_monitor_mqtt_prefix>/button

The payload key 'ev_charging' will "push" the button.ev_smart_charging_manually_start_charging when the value is "start" and "push" the button button ev_smart_charging_manually_stop_charging when the value is "stop".

The payload key 'smart_charging_activated' will set switch.ev_smart_charging_smart_charging_activated "on" / "off".

The payload key 'price_limit' will set number.ev_smart_charging_electricity_price_limit to value provided by the key.
