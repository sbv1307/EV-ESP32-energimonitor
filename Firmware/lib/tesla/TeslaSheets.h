#pragma once

#include <Arduino.h>
#include "../globals/globals.h"

// Sends timestamp + Tesla telemetry + energy counter to Google Sheets.
// Returns true on success.
bool sendTeslaTelemetryToGoogleSheets(TaskParams_t* params, float energyKwh);