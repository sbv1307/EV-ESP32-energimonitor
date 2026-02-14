#pragma once

#include <Arduino.h>
#include "../globals/globals.h"

enum class TeslaSheetTarget {
	TeslaLog,
	TeslaData
};

// Sends a pre-formatted CSV payload to the selected Google Sheets parameter target.
bool sendTeslaPayloadToGoogleSheets(TaskParams_t* params, TeslaSheetTarget target, const String& payload);

// Sends timestamp + Tesla telemetry + energy counter to Google Sheets.
// Returns true on success.
bool sendTeslaTelemetryToGoogleSheets(TaskParams_t* params, float energyKwh);