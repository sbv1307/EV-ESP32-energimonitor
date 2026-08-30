#pragma once

#include <Arduino.h>

/*
 * LED control task for EV-ESP32-energimonitor.
 *
 * Two independently driven LEDs are supported, each backed by its own
 * FreeRTOS queue/task, started lazily on first use and alive for the
 * lifetime of the firmware:
 *  - LedId::Status – boot / WiFi / MQTT connectivity status (GPIO2)
 *  - LedId::Charge – charging state + per-pulse activity (GPIO16)
 *
 * Supported commands
 * ------------------
 *  "Blink"   / "Blink1"  – toggle LED once for a short period, then toggle back.
 *                           Works regardless of the current LED state: if the LED
 *                           is ON the LED goes OFF briefly; if it is OFF it goes
 *                           ON briefly.
 *  "Blink2"  / "Blink3"  – same toggle-and-back sequence repeated N times.
 *  "Toggle"              – blink continuously at ~1 Hz (50 % duty cycle) until
 *                           the next command is received.
 *  "TurnOn"              – turn LED on and keep it on.
 *  "TurnOff"             – turn LED off.
 *
 * Single API:
 *   sendLedCommand(LedId id, const char* command);
 */

enum class LedId : uint8_t { Status = 0, Charge = 1 };

// Start the LED task for 'id' (if not already running) and enqueue 'command'.
// 'command' is copied into the queue; the caller does not need to keep the
// string alive after the call returns.
void sendLedCommand(LedId id, const char* command);

