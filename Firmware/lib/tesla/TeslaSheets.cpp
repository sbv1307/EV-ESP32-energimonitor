#include <Arduino.h>

#include "TeslaSheets.h"
#include "TeslaApi.h"

static bool formatDateTime(char* dateBuf, size_t dateBufLen, char* timeBuf, size_t timeBufLen) {
  struct tm timeinfo;
  bool hasLocalTime = getLocalTime(&timeinfo);

  if (!hasLocalTime) {
    time_t now = time(nullptr);
    if (now > 0) {
      gmtime_r(&now, &timeinfo);
      hasLocalTime = true;
    } else {
      memset(&timeinfo, 0, sizeof(timeinfo));
    }
  }

  // Date: YYYY-MM-DD
  snprintf(dateBuf, dateBufLen, "%04d-%02d-%02d",
           timeinfo.tm_year + 1900,
           timeinfo.tm_mon + 1,
           timeinfo.tm_mday);

  // Time: HH:MM
  snprintf(timeBuf, timeBufLen, "%02d:%02d",
           timeinfo.tm_hour,
           timeinfo.tm_min);

  return hasLocalTime;
}

bool sendTeslaTelemetryToGoogleSheets(TaskParams_t* params, float energyKwh) {
  (void)params;

  TeslaTelemetry telemetry;
  String errorMessage;
  if (!teslaGetTelemetry(&telemetry, &errorMessage)) {
    Serial.println(String("Tesla telemetry fetch failed: ") + errorMessage);
    return false;
  }

  char dateBuf[11] = {0};
  char timeBuf[6] = {0};
  if (!formatDateTime(dateBuf, sizeof(dateBuf), timeBuf, sizeof(timeBuf))) {
    Serial.println("Failed to get local time for telemetry payload; using epoch time");
  }

  // TODO: Replace Serial output with Google Sheets upload.
  Serial.println("--- Tesla Telemetry (for Google Sheets) ---");
  Serial.println(String("Date: ") + dateBuf);
  Serial.println(String("Time: ") + timeBuf);
  Serial.println(String("Battery level (%): ") + telemetry.batteryLevelPercent);
  Serial.println(String("Estimated battery range (mi): ") + telemetry.estimatedBatteryRangeMiles);
  Serial.println(String("Odometer (mi): ") + telemetry.odometerMiles);
  Serial.println(String("Energy (kWh): ") + energyKwh);
  Serial.println(String("Latitude: ") + String(telemetry.latitude, 6));
  Serial.println(String("Longitude: ") + String(telemetry.longitude, 6));
  Serial.println("-----------------------------------------");

  return true;
}