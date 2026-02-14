#pragma once

#include "../globals/globals.h"

void initChargingSession();
void handleChargingSession(TaskParams_t* params);

/* ============================================================================
 * ===============  FOR   TESTING  == To be removed ===========================*/
// For test purpose, to allow the PulseInputIsrTest task to check if the vehicle is currently charging or not, which affects the pulse simulation interval.
// Added function declaration for isChargingSessionCharging, which is used in PulseInputIsrTest.cpp to determine the pulse interval based on whether the vehicle is currently charging or not.
bool isChargingSessionCharging();
/* TEST_ONLY_PULSE_ISR_INTERVAL: END */
