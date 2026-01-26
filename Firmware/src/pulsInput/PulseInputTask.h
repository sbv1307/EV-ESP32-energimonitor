#pragma once
#include "globals/globals.h"
#include <Arduino.h>

void startPulseInputTask(TaskParams_t* params);

void IRAM_ATTR PulseInputISR();

void setPulseCounterFromMqtt(uint32_t newPulseCounter);