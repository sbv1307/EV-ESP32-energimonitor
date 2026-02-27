#pragma once

#include "../globals/globals.h"

void initChargingSession();
void handleChargingSession(TaskParams_t* params);

/* ============================================================================
 * ===============  To be used to display charging status ===========================*/
bool isChargingSessionCharging();

