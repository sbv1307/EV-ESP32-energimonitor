#pragma once

#include <Arduino.h>

constexpr char SKETCH_VERSION[] = "EV-charging ESP32 MQTT interface (TEST VERSION) - V1.0.0";
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
constexpr int CHARGING_ANALOG_THRESHOLD = 2000;
constexpr int CHARGING_ANALOG_HYSTERESIS = 100;
constexpr uint32_t CHARGING_START_CONFIRM_SECONDS = 30;
constexpr uint32_t CHARGING_END_CONFIRM_SECONDS = 30;
constexpr uint32_t CHARGING_ANALOG_SAMPLE_INTERVAL_MS = 1000;