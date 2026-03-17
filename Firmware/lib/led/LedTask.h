#pragma once

#include <Arduino.h>

/*
 * LED_BUILTIN control task for EV-ESP32-energimonitor.
 *
 * The task is command-driven via a FreeRTOS queue.  It is started lazily on
 * the first call to sendLedCommand() and stays alive for the lifetime of the
 * firmware.
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
 *   sendLedCommand(const char* command);
 */

// Start the LED task (if not already running) and enqueue 'command'.
// 'command' is copied into the queue; the caller does not need to keep the
// string alive after the call returns.
void sendLedCommand(const char* command);
