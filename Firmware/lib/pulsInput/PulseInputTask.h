#pragma once
#include "globals.h"
#include <Arduino.h>

constexpr int PULSE_INPUT_TASK_STACK_SIZE = 4785; // 4KB stack size for the task

void startPulseInputTask(TaskParams_t* params);

void IRAM_ATTR PulseInputISR();

void setPulseCounterFromMqtt(uint32_t newPulseCounter);