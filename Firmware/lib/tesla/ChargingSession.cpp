//#define DEBUG_CHARGING_SESSION

#include <Arduino.h>
#include <Preferences.h>
#include <time.h>

#include "ChargingSession.h"
#include "TeslaApi.h"
#include "TeslaSheets.h"
#include "MqttClient.h"
#include "PulseInputTask.h"
#include "config.h"

namespace {
enum class ChargingState {
  Idle,
  StartCandidate,
  Charging,
  EndCandidate
};

struct ChargingSnapshot {
  bool active = false;
  uint64_t startEpoch = 0;
  float startEnergyKwh = 0.0f;
  float startBatteryLevelPercent = 0.0f;
  float startOdometerKm = 0.0f;
};

//static const char* CHARGE_NVS_NAMESPACE = "charging";

static ChargingState gState = ChargingState::Idle;
static ChargingSnapshot gSnapshot;
static bool gSessionInitialized = false;
static bool gHasLastEndEnergy = false;
static float gLastEndEnergyKwh = 0.0f;
static uint32_t gCandidateSinceMs = 0;
static uint32_t gLastSampleMs = 0;
static bool gPendingTeslaDataUpload = false;
static String gPendingTeslaDataPayload;

static void formatDateTimeFromEpoch(uint64_t epochSeconds,
                                    char* dateBuf,
                                    size_t dateBufLen,
                                    char* timeBuf,
                                    size_t timeBufLen) {
  struct tm timeinfo;
  bool valid = false;

  if (epochSeconds > 0) {
    time_t ts = (time_t)epochSeconds;
    valid = localtime_r(&ts, &timeinfo) != nullptr;
  }

  if (!valid) {
    memset(&timeinfo, 0, sizeof(timeinfo));
  }

  snprintf(dateBuf, dateBufLen, "%04d-%02d-%02d",
           timeinfo.tm_year + 1900,
           timeinfo.tm_mon + 1,
           timeinfo.tm_mday);

  snprintf(timeBuf, timeBufLen, "%02d:%02d",
           timeinfo.tm_hour,
           timeinfo.tm_min);
}

static void saveSessionToNvs() {
  Preferences pref;
  pref.begin(CHARGE_NVS_NAMESPACE, false);
  pref.putBool("active", gSnapshot.active);
  pref.putULong64("start_epoch", gSnapshot.startEpoch);
  pref.putFloat("start_energy", gSnapshot.startEnergyKwh);
  pref.putFloat("start_level", gSnapshot.startBatteryLevelPercent);
  pref.putFloat("start_odo", gSnapshot.startOdometerKm);
  pref.putBool("last_known", gHasLastEndEnergy);
  pref.putFloat("last_end", gLastEndEnergyKwh);
  pref.end();
}

static void loadSessionFromNvs() {
  Preferences pref;
  pref.begin(CHARGE_NVS_NAMESPACE, true);
  gSnapshot.active = pref.getBool("active", false);
  gSnapshot.startEpoch = pref.getULong64("start_epoch", 0);
  gSnapshot.startEnergyKwh = pref.getFloat("start_energy", 0.0f);
  gSnapshot.startBatteryLevelPercent = pref.getFloat("start_level", 0.0f);
  gSnapshot.startOdometerKm = pref.getFloat("start_odo", 0.0f);
  gHasLastEndEnergy = pref.getBool("last_known", false);
  gLastEndEnergyKwh = pref.getFloat("last_end", 0.0f);
  pref.end();
}

static bool isStartCondition(int analogValue) {
  return analogValue >= (CHARGING_ANALOG_THRESHOLD + CHARGING_ANALOG_HYSTERESIS);
}

static bool isEndCondition(int analogValue) {
  return analogValue <= (CHARGING_ANALOG_THRESHOLD - CHARGING_ANALOG_HYSTERESIS);
}

static bool candidateDurationReached(uint32_t candidateSinceMs, uint32_t requiredSeconds) {
  if (candidateSinceMs == 0) {
    return false;
  }
  return (millis() - candidateSinceMs) >= (requiredSeconds * 1000UL);
}

static bool createStartSnapshot() {
  TeslaTelemetry telemetry;
  String telemetryError;
  if (!teslaGetTelemetry(&telemetry, &telemetryError)) {

                                                                  #ifdef DEBUG_CHARGING_SESSION
                                                                    char logMsg[128] = {0};
                                                                    snprintf(logMsg, sizeof(logMsg), "Charging start telemetry failed: %s", telemetryError.c_str());
                                                                    Serial.println(logMsg);
                                                                  #endif

    publishMqttLogStatus((String("Charging start telemetry failed: ") + telemetryError).c_str(), false);
    return false;
  }

  float latestEnergyKwh = 0.0f;
  if (!getLatestEnergyKwh(&latestEnergyKwh)) {

                                                                  #ifdef DEBUG_CHARGING_SESSION
                                                                    Serial.println("Charging start energy snapshot failed");
                                                                  #endif

    publishMqttLogStatus("Charging start energy snapshot failed", false);
    return false;
  }

  const float milesToKm = 1.609344f;
  gSnapshot.active = true;
  time_t now = time(nullptr);
  gSnapshot.startEpoch = now > 0 ? (uint64_t)now : 0;
  gSnapshot.startEnergyKwh = latestEnergyKwh;
  gSnapshot.startBatteryLevelPercent = telemetry.batteryLevelPercent;
  gSnapshot.startOdometerKm = telemetry.odometerMiles * milesToKm;

  saveSessionToNvs();

                                                                  #ifdef DEBUG_CHARGING_SESSION
                                                                    Serial.println("Charging start snapshot created");
                                                                  #endif

  publishMqttLog(MQTT_LOG_SUFFIX, "Charging start captured", false);
  return true;
}

static String buildTeslaDataPayload(const TeslaTelemetry& endTelemetry, float endEnergyKwh) {
  const float milesToKm = 1.609344f;

  const float endRangeKm = endTelemetry.estimatedBatteryRangeMiles * milesToKm;
  const float endOdometerKm = endTelemetry.odometerMiles * milesToKm;

  gChargeEnergyKwh = endEnergyKwh - gSnapshot.startEnergyKwh;

  float standbyEnergyKwh = 0.0f;
  bool standbyKnown = false;
  if (gHasLastEndEnergy) {
    standbyEnergyKwh = gSnapshot.startEnergyKwh - gLastEndEnergyKwh;
    standbyKnown = true;
  }

  float whPerKm = 0.0f;
  const float distanceKm = endOdometerKm - gSnapshot.startOdometerKm;
  if (distanceKm > 0.001f) {
    whPerKm = (gChargeEnergyKwh * 1000.0f) / distanceKm;
  }

  char endDateBuf[11] = {0};
  char endTimeBuf[6] = {0};
  char startDateBuf[11] = {0};
  char startTimeBuf[6] = {0};

  time_t now = time(nullptr);
  uint64_t endEpoch = now > 0 ? (uint64_t)now : 0;

  formatDateTimeFromEpoch(endEpoch, endDateBuf, sizeof(endDateBuf), endTimeBuf, sizeof(endTimeBuf));
  formatDateTimeFromEpoch(gSnapshot.startEpoch, startDateBuf, sizeof(startDateBuf), startTimeBuf, sizeof(startTimeBuf));

  String standbyField = standbyKnown ? String(standbyEnergyKwh, 2) : String("");

  String payload = String(endDateBuf) + "," +
                   String(endTimeBuf) + "," +
                   String(startTimeBuf) + "," +
                   String(endEnergyKwh, 2) + "," +
                   standbyField + "," +
                   String(gChargeEnergyKwh, 2) + "," +
                   String(gSnapshot.startBatteryLevelPercent, 1) + "," +
                   String(endTelemetry.batteryLevelPercent, 1) + "," +
                   String(endRangeKm, 2) + "," +
                   String(endOdometerKm, 0) + "," +
                   String(whPerKm, 1) + "," +
                   String(endTelemetry.latitude, 6) + "," +
                   String(endTelemetry.longitude, 6);

  return payload;
}

static bool finalizeChargingSession(TaskParams_t* params) {
  TeslaTelemetry endTelemetry;
  String telemetryError;
  if (!teslaGetTelemetry(&endTelemetry, &telemetryError)) {
    publishMqttLogStatus((String("Charging end telemetry failed: ") + telemetryError).c_str(), false);
    return false;
  }

  float endEnergyKwh = 0.0f;
  if (!getLatestEnergyKwh(&endEnergyKwh)) {
    publishMqttLogStatus("Charging end energy snapshot failed", false);
    return false;
  }

  String payload = buildTeslaDataPayload(endTelemetry, endEnergyKwh);

  if (!sendTeslaPayloadToGoogleSheets(params, TeslaSheetTarget::TeslaData, payload)) {
    gPendingTeslaDataPayload = payload;
    gPendingTeslaDataUpload = true;

                                                                #ifdef DEBUG_CHARGING_SESSION
                                                                  Serial.println("TeslaData upload pending (WiFi/API)");
                                                                #endif

    publishMqttLog(MQTT_LOG_SUFFIX, "TeslaData upload pending (WiFi/API)", false);
  } else {

                                                                #ifdef DEBUG_CHARGING_SESSION
                                                                  Serial.println("TeslaData upload sent");  
                                                                #endif

    publishMqttLog(MQTT_LOG_SUFFIX, "TeslaData upload sent", false);
  }

  gLastEndEnergyKwh = endEnergyKwh;
  gHasLastEndEnergy = true;

  gSnapshot.active = false;
  gSnapshot.startEpoch = 0;
  gSnapshot.startEnergyKwh = 0.0f;
  gSnapshot.startBatteryLevelPercent = 0.0f;
  gSnapshot.startOdometerKm = 0.0f;

  saveSessionToNvs();
  return true;
}

static void trySendPendingTeslaData(TaskParams_t* params) {
  if (!gPendingTeslaDataUpload || gPendingTeslaDataPayload.isEmpty()) {
    return;
  }

  if (sendTeslaPayloadToGoogleSheets(params, TeslaSheetTarget::TeslaData, gPendingTeslaDataPayload)) {
    gPendingTeslaDataUpload = false;
    gPendingTeslaDataPayload = "";
    publishMqttLog(MQTT_LOG_SUFFIX, "Pending TeslaData upload sent", false);
  }
}
} // namespace

void initChargingSession() {
  if (gSessionInitialized) {
    return;
  }

  loadSessionFromNvs();

  if (CHARGING_ANALOG_PIN >= 0) {
    pinMode(CHARGING_ANALOG_PIN, INPUT);
    analogReadResolution(12); // 0-4095 range for 12-bit ADC
  }

  if (gSnapshot.active) {
    gState = ChargingState::Charging;

                                                                #ifdef DEBUG_CHARGING_SESSION
                                                                Serial.println("Charging session restored from NVS");
                                                                #endif

    publishMqttLog(MQTT_LOG_SUFFIX, "Charging session restored from NVS", false);
  } else {
    gState = ChargingState::Idle;
  }

  gSessionInitialized = true;
}

void handleChargingSession(TaskParams_t* params) {
  if (CHARGING_ANALOG_PIN < 0) {
    return;
  }

  if (!gSessionInitialized) {
    initChargingSession();
  }

  trySendPendingTeslaData(params);

  uint32_t nowMs = millis();
  if ((nowMs - gLastSampleMs) < CHARGING_ANALOG_SAMPLE_INTERVAL_MS) {
    return;
  }
  gLastSampleMs = nowMs;

  int analogValue = analogRead(CHARGING_ANALOG_PIN);

                                                            #ifdef DEBUG_CHARGING_SESSION
                                                            static int lastLoggedAnalogValue = -1;
                                                            if (analogValue != lastLoggedAnalogValue) {
                                                              Serial.print("ChargingSession.cpp: Charging session state: ");
                                                              switch (gState) {
                                                                case ChargingState::Idle:
                                                                  Serial.print("Idle");
                                                                  break;
                                                                case ChargingState::StartCandidate:
                                                                  Serial.print("StartCandidate");
                                                                  break;
                                                                case ChargingState::Charging:
                                                                  Serial.print("Charging");
                                                                  break;
                                                                case ChargingState::EndCandidate:
                                                                  Serial.print("EndCandidate");
                                                                  break;
                                                              }
                                                              Serial.println();
                                                              Serial.print("ChargingSession.cpp: Charging threshold: ");
                                                              Serial.print(CHARGING_ANALOG_THRESHOLD);
                                                              Serial.print(" Analog value: ");
                                                              Serial.println(analogValue);
                                                              lastLoggedAnalogValue = analogValue;
                                                            }
                                                            #endif

  switch (gState) {
    case ChargingState::Idle:
      if (isStartCondition(analogValue)) {
        gState = ChargingState::StartCandidate;
        gCandidateSinceMs = nowMs;
      }
      break;

    case ChargingState::StartCandidate:
      if (!isStartCondition(analogValue)) {
        gState = ChargingState::Idle;
        gCandidateSinceMs = 0;
        break;
      }
      if (candidateDurationReached(gCandidateSinceMs, CHARGING_START_CONFIRM_SECONDS)) {
        if (createStartSnapshot()) {
          gState = ChargingState::Charging;
        } else {
          gState = ChargingState::Idle;
        }
        gCandidateSinceMs = 0;
      }
      break;

    case ChargingState::Charging:
      if (isEndCondition(analogValue)) {
        gState = ChargingState::EndCandidate;
        gCandidateSinceMs = nowMs;
      }
      break;

    case ChargingState::EndCandidate:
      if (!isEndCondition(analogValue)) {
        gState = ChargingState::Charging;
        gCandidateSinceMs = 0;
        break;
      }
      if (candidateDurationReached(gCandidateSinceMs, CHARGING_END_CONFIRM_SECONDS)) {
        if (finalizeChargingSession(params)) {
          gState = ChargingState::Idle;
        } else {
          gState = ChargingState::Charging;
        }
        gCandidateSinceMs = 0;
      }
      break;
  }
}

/* ============================================================================
 * ===============  To be used to display charging status ===========================*/

bool isChargingSessionCharging() {
  return gState == ChargingState::Charging;
}
