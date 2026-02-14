//#define DEBUG
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "TeslaSheets.h"
#include "TeslaApi.h"
#include "config.h"
#include "privateConfig.h"

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

bool sendTeslaPayloadToGoogleSheets(TaskParams_t* params, TeslaSheetTarget target, const String& payload) {
  (void)params;

  const char* sheetParamName = (target == TeslaSheetTarget::TeslaData)
                                  ? TESLA_GSHEET_PARAM_NAME_DATA
                                  : TESLA_GSHEET_PARAM_NAME;

  String url = String(TESLA_GSHEET_WEBAPP_URL_PREFIX) +
               TESLA_GSHEET_WEBAPP_DEPLOYMENT_ID +
               TESLA_GSHEET_WEBAPP_URL_SUFFIX +
               "?" + sheetParamName + "=" + payload;

                                                            #ifdef DEBUG
                                                            Serial.print("Uploading to Google Sheets: ");
                                                            Serial.println(url);
                                                            #endif 

  if (WiFi.status() != WL_CONNECTED) {

                                                            #ifdef DEBUG
                                                            Serial.println("Google Sheets upload failed: WiFi not connected");
                                                            #endif

    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(20000);
  if (!http.begin(client, url)) {

                                                              #ifdef DEBUG
                                                              Serial.println("Google Sheets upload failed: HTTP begin failed");
                                                              #endif

    return false;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {

                                                              #ifdef DEBUG
                                                              Serial.println(String("Google Sheets upload failed: HTTP GET failed with code ") + httpCode);
                                                              #endif

    http.end();
    return false;
  }

  http.end();
  return true;
}

bool sendTeslaTelemetryToGoogleSheets(TaskParams_t* params, float energyKwh) {
  TeslaTelemetry telemetry;
  String errorMessage;
  if (!teslaGetTelemetry(&telemetry, &errorMessage)) {

                                                  #ifdef DEBUG
                                                  Serial.println(String("Tesla telemetry fetch failed: ") + errorMessage);
                                                  #endif

    return false;
  }

  char dateBuf[11] = {0};
  char timeBuf[6] = {0};
  if (!formatDateTime(dateBuf, sizeof(dateBuf), timeBuf, sizeof(timeBuf))) {
                                                  #ifdef DEBUG
                                                  Serial.println("Failed to get local time for telemetry payload; using epoch time");
                                                  #endif
  }

  const float milesToKm = 1.609344f;
  const float rangeKm = telemetry.estimatedBatteryRangeMiles * milesToKm;
  const float odometerKm = telemetry.odometerMiles * milesToKm;

  String payload = String(dateBuf) + "," +
                   String(timeBuf) + "," +
                   String(telemetry.batteryLevelPercent, 1) + "," +
                   String(rangeKm, 2) + "," +
                   String(odometerKm, 0) + "," +
                   String(energyKwh, 2) + "," +
                   String(telemetry.latitude, 6) + "," +
                   String(telemetry.longitude, 6);

  return sendTeslaPayloadToGoogleSheets(params, TeslaSheetTarget::TeslaLog, payload);
}