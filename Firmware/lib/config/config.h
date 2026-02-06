#pragma once

#include <Arduino.h>

constexpr char SKETCH_VERSION[] = "EV-charging ESP32 MQTT interface - V1.0.0";
constexpr char NVS_NAMESPACE[] = "config"; // Namespace for NVS storage
constexpr int PULSE_INPUT_GPIO = -1; // Set to the GPIO used for pulse input.
constexpr int PULSE_INPUT_INTERRUPT_MODE = FALLING;