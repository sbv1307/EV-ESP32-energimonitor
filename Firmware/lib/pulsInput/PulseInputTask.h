#pragma once
#include "globals.h"
#include <Arduino.h>

void startPulseInputTask(TaskParams_t* params);

void IRAM_ATTR PulseInputISR();

bool isPulseInputReady();

bool waitForPulseInputReady(uint32_t timeoutMs);

bool attachPulseInputInterrupt(int gpio, int mode);

void setPulseCounterFromMqtt(uint32_t newPulseCounter);

bool getLatestEnergyKwh(float* energyKwh);

bool getLatestEnergySnapshot(float* powerW, float* energyKwh, float* subtotalKwh);

void requestSubtotalReset();