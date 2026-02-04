#pragma once

#include <Arduino.h>

struct TeslaTelemetry {
  float  estimatedBatteryRangeMiles = 0.0f;
  float  batteryLevelPercent = 0.0f;
  float  odometerMiles = 0.0f;
  double latitude = 0.0;
  double longitude = 0.0;
  bool   isValid = false;
};

// Fetch battery range, odometer, and GPS coordinates from Tesla Owner API.
// Returns true on success and populates `outTelemetry`.
// Values are in miles (per Tesla API) and degrees for latitude/longitude.
bool teslaGetTelemetry(TeslaTelemetry* outTelemetry, String* errorMessage = nullptr);