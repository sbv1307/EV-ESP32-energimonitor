#pragma once

#include <Arduino.h>

constexpr char SKETCH_VERSION[] = "EV-charging ESP32 MQTT interface (TEST VERSION) - V1.0.0";
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

constexpr int PULSE_INPUT_GPIO = -1; // Set to the GPIO used for pulse input.
constexpr int PULSE_INPUT_INTERRUPT_MODE = FALLING;

// Charging session trigger (analog input based)
/*
 * ESP32 limitation: when Wi-Fi is active, ADC2 pins cannot be used reliably with analogRead().
 * CHARGING_ANALOG_PIN should therefore be set to an ADC1 pin (GPIO32-39) to avoid ESP_ERR_TIMEOUT.
 */
constexpr int CHARGING_ANALOG_PIN = 34; // Set to ADC-capable GPIO to enable charging state machine.
constexpr int CHARGING_ANALOG_THRESHOLD = 2000; // Analog threshold for detecting charging state. Adjust based on testing with your specific hardware setup and voltage divider if used.
constexpr int CHARGING_ANALOG_HYSTERESIS = 100; // Hysteresis value to prevent rapid toggling of charging state.
constexpr uint32_t CHARGING_START_CONFIRM_SECONDS = 10; // Number of seconds the analog value must continuously indicate charging start before confirming session start
constexpr uint32_t CHARGING_END_CONFIRM_SECONDS = 10; // Number of seconds the analog value must continuously indicate charging end before confirming session end
constexpr uint32_t CHARGING_ANALOG_SAMPLE_INTERVAL_MS = 1000; // Interval in milliseconds between analog samples for charging detection