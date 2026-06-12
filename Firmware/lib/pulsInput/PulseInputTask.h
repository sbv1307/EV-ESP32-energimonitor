#pragma once
#include "globals.h"
#include <Arduino.h>

void startPulseInputTask(TaskParams_t* params);

void initResetGpioPins(); // Initialize HARD_RESET_GPIO early in boot (before task startup)

void IRAM_ATTR PulseInputISR();

bool isPulseInputReady();

bool waitForPulseInputReady(uint32_t timeoutMs);

bool attachPulseInputInterrupt(int gpio, int mode);

void suspendPulseInputISR(); // Detach pulse interrupt (call during OTA)
void resumePulseInputISR();  // Re-attach pulse interrupt (call after OTA)

void suspendDirectResetISR(); // Detach direct-reset interrupt (call during OTA)
void resumeDirectResetISR();  // Re-attach direct-reset interrupt (call after OTA)

void setPulseCounterFromMqtt(uint32_t newPulseCounter);

bool getLatestEnergyKwh(float* energyKwh);

bool getLatestEnergySnapshot(float* powerW, float* energyKwh, float* subtotalKwh);

void requestSubtotalReset();

typedef enum {
  RESET_SOFT,
  RESET_HARD,
} ResetType_t;

void requestReset(ResetType_t type);