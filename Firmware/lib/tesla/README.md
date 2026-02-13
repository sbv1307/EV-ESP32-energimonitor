I will extend the project to measure charging telemetry.
This will be done in two phases:
1: Register 'start charging'
2: Register 'end charging'

'start charging' will happen when the level from reading an analog input, extend a configurable setting + a Hysteresis and stays there for 30 seconds (configurable).
'end charging' will happen when the level from reading an analog input, falls below the configurable setting - the Hysteresis and stays there for 30 seconds (configurable).

When charging starts ('start charging'), the following data are store for use when charging ends ('end charging'):
current time (start time), LatestEnergyKwh (start energy kWh), current batteryLevelPercent (start level) and current odometerKm (start odometerKm).
The collected data will be stored in NVS in order to handle outage during charging.

When charging ends ('end charging'), the following data will be send to google sheets:
date, current time (end time), 'start time', LatestEnergyKwh (end energy kWh), ('start energy kWh' - 'last energy kWh' (if known)), ('end energy kWh' - 'start energy kWh'), 'start level', current batteryLevelPercent (end level), rangeKm, current odometerKm (end odometerKm), (('end energy kWh' - 'start energy kWh') / ('end odometerKm' - 'start odometerKm')), latitude and longitude.
The current function sendTeslaTelemetryToGoogleSheets() will be modified to handle the above transmission as well as most of the data are the same.
current LatestEnergyKwh (end energy kWh) will then be saved to NVS as 'last energy kWh' for next time charging ends.
