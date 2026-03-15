#pragma once

#include <Arduino.h>

/*
 * Push-button library for EV-ESP32-energimonitor.
 *
 * Each button is wired active-low (to GND) and uses the internal pull-up.
 * An ISR fires on the falling edge and queues a ButtonCommand into a FreeRTOS
 * queue with ISR-level hardware debounce via esp_timer_get_time().
 *
 * processPushButtonCommands() is intended to be called from networkTask().
 * It drains the queue and spawns a one-shot FreeRTOS task per command that
 * reads the current global state, builds the JSON payload, and publishes it
 * via publishMqttSetCommand().
 */

// Call once from setup() after Serial and GPIO subsystems are ready.
void initPushButtons();

// Call from networkTask() to drain the button command queue and spawn publish tasks.
void processPushButtonCommands();
