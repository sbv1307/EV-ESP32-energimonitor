#pragma once
#include "globals.h"
#include <Arduino.h>

constexpr int PULSE_INPUT_TASK_STACK_SIZE = 2387; // 4KB stack size for the task

void startPulseInputTask(TaskParams_t* params);

void IRAM_ATTR PulseInputISR();

bool isPulseInputReady();

bool attachPulseInputInterrupt(int gpio, int mode);

void setPulseCounterFromMqtt(uint32_t newPulseCounter);

bool getLatestEnergyKwh(float* energyKwh);

void requestSubtotalReset();