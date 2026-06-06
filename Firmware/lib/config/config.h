#pragma once

#include <Arduino.h>

constexpr char SKETCH_VERSION[] = "EV-charging ESP32 MQTT monitor interface - V4.2.4";
/*
 * About NVS (Non-Volatile Storage)
 * NVS is used to store configuration, pulse counter, and charging session data persistently.
 * Each logical grouping of data has its own NVS namespace to avoid conflicts and allow for independent management:
- CONFIG_NVS_NAMESPACE: Used for general configuration settings (e.g., WiFi credentials, MQTT settings).
- COUNT_NVS_NAMESPACE: Used specifically for storing the pulse counter and subtotal in the PulseInputTask.
- CHARGE_NVS_NAMESPACE: Used for storing the current charging session state and snapshot in the ChargingSession module.
- TESLA_PREF_NVS_NAMESPACE: Used for storing Tesla API related preferences such as GPIO pins and thresholds.
 * This separation allows for better organization and reduces the risk of accidentally overwriting unrelated data.
 * NOTE: NVS and data stored will not be cleared on OTA updates, so it is important to manage stored data carefully and 
 * consider versioning if the structure of stored data changes in future updates.
 * Do not change the namespace names lightly, as they are used in multiple places across the codebase for reading and writing data.
 * If you need to change a namespace, ensure you update all references to it in the codebase and consider how to handle existing stored data (e.g., migration or clearing).
 * For example, if you change CHARGE_NVS_NAMESPACE, you would need to update the namespace used in both saveSessionToNvs() and loadSessionFromNvs() functions in ChargingSession.cpp, as well as any other place where this namespace is referenced for reading or writing charging session data.
 * If you need to clear stored data for testing or development purposes, you can use the "Erase Flash" option in the Arduino IDE before uploading new firmware, but be cautious as this will clear all stored data across all namespaces.
 * In production use, consider implementing a versioning system for stored data to allow for smooth transitions
 */
constexpr char CONFIG_NVS_NAMESPACE[] = "config"; // globals.cpp: Namespace for NVS storage
constexpr char COUNT_NVS_NAMESPACE[] = "storage"; // PulseInputTask.cpp: Namespace for NVS storage of pulse counter and subtotal
constexpr char CHARGE_NVS_NAMESPACE[] = "charging"; // ChargingSession.cpp: Charge session state and snapshot storage
constexpr char TESLA_PREF_NVS_NAMESPACE[] = "tesla"; // TeslaApi.cpp: GPIO and thresholds for pulse input (energy meter)

constexpr int PULSE_INPUT_GPIO = 33; /* PULSE_INPUT_GPIO = 33
                                        Open-collector output requires an internal (or external) pull-up. 
                                        GPIO 33 is interrupt-capable, ADC1, and has internal pull-up support — perfect 
                                        for this. Configure as INPUT_PULLUP.*/
constexpr int PULSE_INPUT_INTERRUPT_MODE = FALLING;

// Charging session trigger (analog input based)
/*
 * ESP32 limitation: when Wi-Fi is active, ADC2 pins cannot be used reliably with analogRead().
 * CHARGING_ANALOG_GPIO should therefore be set to an ADC1 pin (GPIO32-39) to avoid ESP_ERR_TIMEOUT.
 *
 * AC current sensing (SCT01-T10/50A):
 * The SCT01-T10/50A has a built-in burden resistor and outputs an AC voltage, so no external burden
 * resistor is needed. The signal still needs to be biased near VCC/2 (~1.65 V) before it is fed
 * into the ESP32 ADC. readAcRms() samples the pin CHARGING_AC_SAMPLE_COUNT times (one per ms),
 * removes the measured DC offset from that window, and returns the RMS amplitude as a 12-bit ADC count.
 * CHARGING_ANALOG_THRESHOLD and CHARGING_ANALOG_HYSTERESIS are compared against this RMS value;
 * adjust them based on testing with your specific installation and charging current.
 */
constexpr int CHARGING_ANALOG_GPIO = 34; // Set to ADC1-capable GPIO (GPIO32-39) to enable charging state machine.
constexpr int CHARGING_AC_SAMPLE_COUNT = 100; // Samples per RMS measurement (1 ms each → 100 ms window ≈ 5 cycles @ 50 Hz)
constexpr int CHARGING_ANALOG_THRESHOLD = 40; // Start value tuned for SCT01-T10/50A with charging start around 900 W (~3.9 A @ 230 V). Adjust on-site if needed.
constexpr int CHARGING_ANALOG_HYSTERESIS = 12; // Hysteresis in RMS ADC counts; keeps start/stop stable while still detecting around-threshold charging transitions.
constexpr uint32_t CHARGING_START_CONFIRM_SECONDS = 5; // Number of seconds the analog value must continuously indicate charging start before confirming session start
constexpr uint32_t CHARGING_END_CONFIRM_SECONDS = 5; // Number of seconds the analog value must continuously indicate charging end before confirming session end
constexpr uint32_t CHARGING_ANALOG_SAMPLE_INTERVAL_MS = 1000; // Interval in milliseconds between analog samples for charging detection

// Push-button GPIO assignments (-1 = disabled)
constexpr int BUTTON_EV_CHARGING_TOGGLE_GPIO    = 14;  // GPIO for EV charging start/stop toggle button
constexpr int BUTTON_SMART_CHARGING_TOGGLE_GPIO = 25;  // GPIO for smart charging on/off toggle button
constexpr int BUTTON_PRICE_LIMIT_INCREASE_GPIO  = 26;  // GPIO for price-limit increase button
constexpr int BUTTON_PRICE_LIMIT_DECREASE_GPIO  = 27;  // GPIO for price-limit decrease button

constexpr bool     BUTTON_ACTIVE_LOW      = true;   // true = INPUT_PULLUP, interrupt on FALLING edge
constexpr uint32_t BUTTON_DEBOUNCE_MS     = 40;     // Minimum ms between accepted button presses
constexpr float    BUTTON_PRICE_LIMIT_STEP = 0.10f; // Price-limit increment/decrement per button press

// Reset GPIO assignments (-1 = disabled)
constexpr int HARD_RESET_GPIO   = 13; // Output GPIO driven LOW to trigger external power-cycle hardware
constexpr int DIRECT_RESET_GPIO = 32; // Input GPIO for power-fail signal; triggers emergency NVS save before power loss
                                       // GPIO 32: ADC1, interrupt-capable, internal pull-up supported (unlike GPIO 34-39).
                                       // Requires PCB trace routed to GPIO 32 (not GPIO 35).

// Unexpected power-outage recovery
// After an unexpected power loss (ESP_RST_POWERON or ESP_RST_BROWNOUT without a prior controlled-reset flag),
// the device waits POWER_OUTAGE_REBOOT_DELAY_MS before performing a controlled hard reset.
// This gives routers and other network devices time to finish booting, so the ESP32 can connect
// to MQTT and WiFi on its next startup.
constexpr uint32_t POWER_OUTAGE_REBOOT_DELAY_MS = 30UL * 60UL * 1000UL; // 30-minute delay (ms)
constexpr char     POWER_OUTAGE_NVS_KEY[]        = "ctrl_rst";            // NVS key (bool) in CONFIG_NVS_NAMESPACE: true = controlled hard reset pending/done