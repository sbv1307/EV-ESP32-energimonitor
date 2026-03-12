#pragma once

#include <Arduino.h>
#include "../globals/globals.h"

enum class TeslaSheetTarget {
	TeslaLog,
	TeslaData
};

// Sends a pre-formatted CSV payload to the selected Google Sheets parameter target.
bool sendTeslaPayloadToGoogleSheets(TaskParams_t* params, TeslaSheetTarget target, const char* payload);
inline bool sendTeslaPayloadToGoogleSheets(TaskParams_t* params, TeslaSheetTarget target, const String& payload) {
	return sendTeslaPayloadToGoogleSheets(params, target, payload.c_str());
}

// Sends timestamp + Tesla telemetry + energy counter to Google Sheets.
// Returns true on success.
bool sendTeslaTelemetryToGoogleSheets(TaskParams_t* params, float energyKwh);

// Starts a one-shot task for asynchronous telemetry upload.
// Returns false if task creation failed.
bool passTeslaTelemetryToGoogleSheets(TaskParams_t* params, float energyKwh);