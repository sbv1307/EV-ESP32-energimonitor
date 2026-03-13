//#define DEBUG
#define STACK_WATERMARK

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "TeslaSheets.h"
#include "TeslaApi.h"
#include "config.h"
#include "privateConfig.h"

namespace {
constexpr size_t TESLA_COMMENT_BUFFER_SIZE = 16;

struct TeslaTelemetryQueueItem {
  TaskParams_t* params = nullptr;
  float energyKwh = 0.0f;
  char comment[TESLA_COMMENT_BUFFER_SIZE] = {0};
};

// constexpr uint32_t TESLA_TELEMETRY_TASK_STACK_SIZE = 8192; // '//'TOBE REMOVED after testing TESLA_TELEMETRY_TASK_STACK_SIZE
constexpr UBaseType_t TESLA_TELEMETRY_TASK_PRIORITY = 1;
static portMUX_TYPE teslaTelemetryTaskStateMux = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t teslaTelemetryTaskHandle = nullptr;
constexpr size_t TESLA_PAYLOAD_BUFFER_SIZE = 224;
constexpr size_t TESLA_URL_BUFFER_SIZE = 640;

static void sendTeslaTelemetryToGoogleSheetsTask(void* pvParameters) {
  TeslaTelemetryQueueItem* item = static_cast<TeslaTelemetryQueueItem*>(pvParameters);
  if (item != nullptr) {
    sendTeslaTelemetryToGoogleSheets(item->params, item->energyKwh, item->comment);
    delete item;
  }

                                                            #ifdef STACK_WATERMARK
                                                            gTeslaTaskStackHighWater = uxTaskGetStackHighWaterMark(nullptr);
                                                            #endif

  portENTER_CRITICAL(&teslaTelemetryTaskStateMux);
  teslaTelemetryTaskHandle = nullptr;
  portEXIT_CRITICAL(&teslaTelemetryTaskStateMux);

  vTaskDelete(nullptr);
}
}

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

bool sendTeslaPayloadToGoogleSheets(TaskParams_t* params, TeslaSheetTarget target, const char* payload) {
  (void)params;

  const char* sheetParamName = (target == TeslaSheetTarget::TeslaData)
                                  ? TESLA_GSHEET_PARAM_NAME_DATA
                                  : TESLA_GSHEET_PARAM_NAME_LOG;

  char url[TESLA_URL_BUFFER_SIZE] = {0};
  const int urlLen = snprintf(
      url,
      sizeof(url),
      "%s%s%s?%s=%s",
      TESLA_GSHEET_WEBAPP_URL_PREFIX,
      TESLA_GSHEET_WEBAPP_DEPLOYMENT_ID,
      TESLA_GSHEET_WEBAPP_URL_SUFFIX,
      sheetParamName,
      payload);

  if (urlLen < 0 || static_cast<size_t>(urlLen) >= sizeof(url)) {

                                                            #ifdef DEBUG
                                                            Serial.println("Google Sheets upload failed: URL buffer overflow");
                                                            #endif

    return false;
  }

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
  String responseBody = http.getString();
  responseBody.trim();
  bool requestSucceeded = (httpCode == HTTP_CODE_OK) && responseBody.equalsIgnoreCase("OK");

  if (!requestSucceeded) {

                                                              #ifdef DEBUG
                                                              Serial.print("Google Sheets upload failed: HTTP GET failed with code ");
                                                              Serial.println(httpCode);
                                                              Serial.print("Response body: ");
                                                              Serial.println(responseBody);
                                                              #endif

    http.end();
    return false;
  }

  http.end();
  return true;
}

bool sendTeslaTelemetryToGoogleSheets(TaskParams_t* params, float energyKwh, const char* comment) {
  TeslaTelemetry telemetry;
  String errorMessage;
  if (!teslaGetTelemetry(&telemetry, &errorMessage)) {

                                                  #ifdef DEBUG
                                                  Serial.print("Tesla telemetry fetch failed: ");
                                                  Serial.println(errorMessage);
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
  char telemetryComment[TESLA_COMMENT_BUFFER_SIZE] = {0};
  snprintf(telemetryComment, sizeof(telemetryComment), "%s", (comment != nullptr) ? comment : "");

  char payload[TESLA_PAYLOAD_BUFFER_SIZE] = {0};
  const int payloadLen = snprintf(
      payload,
      sizeof(payload),
      "%s,%s,%.1f,%.2f,%.0f,%.2f,%.6f,%.6f,%s",
      dateBuf,
      timeBuf,
      telemetry.batteryLevelPercent,
      rangeKm,
      odometerKm,
      energyKwh,
      telemetry.latitude,
      telemetry.longitude,
      telemetryComment);

  if (payloadLen < 0 || static_cast<size_t>(payloadLen) >= sizeof(payload)) {

                                                  #ifdef DEBUG
                                                  Serial.println("Google Sheets upload failed: payload buffer overflow");
                                                  #endif

    return false;
  }

  return sendTeslaPayloadToGoogleSheets(params, TeslaSheetTarget::TeslaLog, payload);
}

/*
 * NOTE: const char* comment has a limit in number of characters defined by TESLA_COMMENT_BUFFER_SIZE.
 * Further comment does not accept spaces or any special characters! Use e.g. BootTelemetrty or Boot_Telemetry instead
*/
bool passTeslaTelemetryToGoogleSheets(TaskParams_t* params, float energyKwh, const char* comment) {
  portENTER_CRITICAL(&teslaTelemetryTaskStateMux);
  if (teslaTelemetryTaskHandle != nullptr) {
    portEXIT_CRITICAL(&teslaTelemetryTaskStateMux);
    return false;
  }
  portEXIT_CRITICAL(&teslaTelemetryTaskStateMux);

  TeslaTelemetryQueueItem* item = new TeslaTelemetryQueueItem;
  /* Note: If the allocation fails, it returns nullptr. We should check for that before proceeding.
   * item can be nullptr only if allocation fails (or a custom allocator returns null).
   * 
   * Most likely causes on ESP32/Arduino:
   * 
   * Heap exhausted: not enough free RAM when new runs.
   * Heap fragmentation: total free heap exists, but no contiguous block large enough.
   * Transient memory pressure: other tasks/allocations briefly consume heap at that moment.
   * Allocator/heap corruption: prior out-of-bounds write or double-free breaks allocator state.
   * Custom operator new behavior: in many embedded builds, new is non-throwing and may return nullptr on OOM.
   * 
   * It might be rare to hit this in practice unless the system is under heavy memory pressure or fragmentation, 
   * but it's good to check to avoid dereferencing a null pointer and crashing. 
   * If allocation fails, we return false to indicate we couldn't enqueue the telemetry.
   * 
   * It might be worth adding logging here in debug builds to help identify if/when this happens, 
   * as it could indicate memory issues that need to be addressed.
   */
  if (item == nullptr) {
    return false;
  }

  item->params = params;
  item->energyKwh = energyKwh;
  snprintf(item->comment, sizeof(item->comment), "%s", (comment != nullptr) ? comment : "");

  BaseType_t result = xTaskCreate(
      sendTeslaTelemetryToGoogleSheetsTask,
      "TeslaSheetsTask",
      TESLA_TELEMETRY_TASK_STACK_SIZE,
      item,
      TESLA_TELEMETRY_TASK_PRIORITY,
      &teslaTelemetryTaskHandle);

  if (result != pdPASS) {
    delete item;
    portENTER_CRITICAL(&teslaTelemetryTaskStateMux);
    teslaTelemetryTaskHandle = nullptr;
    portEXIT_CRITICAL(&teslaTelemetryTaskStateMux);
    return false;
  }

  return true;
}