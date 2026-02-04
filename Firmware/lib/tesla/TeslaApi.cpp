#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "TeslaApi.h"
#include "config.h"
#include "privateConfig.h"

static const char* TESLA_API_BASE_URL = "https://owner-api.teslamotors.com/api/1";
static const char* TESLA_AUTH_URL = "https://auth.tesla.com/oauth2/v3/token";
static const char* TESLA_PREF_NAMESPACE = "tesla";

static bool teslaParseChargeState(const String& json, TeslaTelemetry* telemetry, bool* hasRange = nullptr, bool* hasBatteryLevel = nullptr);
static bool teslaParseVehicleState(const String& json, TeslaTelemetry* telemetry, bool* hasOdometer = nullptr);

struct TeslaAuthState {
  String accessToken;
  String refreshToken;
  uint64_t expiresAt = 0; // epoch seconds
  bool initialized = false;
};

static TeslaAuthState gTeslaAuth;

static void teslaStoreTokens() {
  Preferences pref;
  pref.begin(TESLA_PREF_NAMESPACE, false);
  pref.putString("access_token", gTeslaAuth.accessToken);
  pref.putString("refresh_token", gTeslaAuth.refreshToken);
  pref.putULong64("expires_at", gTeslaAuth.expiresAt);
  pref.end();
}

static void teslaLoadTokens() {
  if (gTeslaAuth.initialized) {
    return;
  }

  Preferences pref;
  pref.begin(TESLA_PREF_NAMESPACE, false);
  gTeslaAuth.accessToken = pref.getString("access_token", "");
  gTeslaAuth.refreshToken = pref.getString("refresh_token", "");
  gTeslaAuth.expiresAt = pref.getULong64("expires_at", 0);
  pref.end();

  if (gTeslaAuth.accessToken.isEmpty() && strlen(TESLA_ACCESS_TOKEN) > 0) {
    gTeslaAuth.accessToken = TESLA_ACCESS_TOKEN;
  }
  if (gTeslaAuth.refreshToken.isEmpty() && strlen(TESLA_REFRESH_TOKEN) > 0) {
    gTeslaAuth.refreshToken = TESLA_REFRESH_TOKEN;
  }

  if (!gTeslaAuth.accessToken.isEmpty() || !gTeslaAuth.refreshToken.isEmpty()) {
    teslaStoreTokens();
  }

  gTeslaAuth.initialized = true;
}

static bool teslaRefreshAccessToken(String* errorMessage) {
  teslaLoadTokens();

  if (gTeslaAuth.refreshToken.isEmpty()) {
    if (errorMessage) {
      *errorMessage = "Tesla refresh token not configured";
    }
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure(); // TODO: Replace with proper root CA for production use.

  HTTPClient http;
  if (!http.begin(client, TESLA_AUTH_URL)) {
    if (errorMessage) {
      *errorMessage = "HTTP begin failed (auth)";
    }
    return false;
  }

  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String body = "grant_type=refresh_token";
  body += "&client_id=" + String(TESLA_CLIENT_ID);
  if (strlen(TESLA_CLIENT_SECRET) > 0) {
    body += "&client_secret=" + String(TESLA_CLIENT_SECRET);
  }
  body += "&refresh_token=" + gTeslaAuth.refreshToken;

  int httpCode = http.POST(body);
  if (httpCode <= 0) {
    if (errorMessage) {
      *errorMessage = String("HTTP POST failed: ") + http.errorToString(httpCode);
    }
    http.end();
    return false;
  }

  if (httpCode != HTTP_CODE_OK) {
    if (errorMessage) {
      *errorMessage = String("Auth HTTP status ") + httpCode + ": " + http.getString();
    }
    http.end();
    return false;
  }

  String response = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, response);
  if (err) {
    if (errorMessage) {
      *errorMessage = String("Auth JSON parse failed: ") + err.c_str();
    }
    return false;
  }

  if (doc["access_token"].isNull()) {
    if (errorMessage) {
      *errorMessage = "Auth response missing access_token";
    }
    return false;
  }

  gTeslaAuth.accessToken = doc["access_token"].as<String>();
  if (!doc["refresh_token"].isNull()) {
    gTeslaAuth.refreshToken = doc["refresh_token"].as<String>();
  }

  uint32_t expiresIn = doc["expires_in"].isNull() ? 0 : doc["expires_in"].as<uint32_t>();
  time_t now = time(nullptr);
  if (now > 0 && expiresIn > 0) {
    gTeslaAuth.expiresAt = (uint64_t)now + (uint64_t)expiresIn - 60; // refresh 1 min early
  } else {
    gTeslaAuth.expiresAt = 0;
  }

  teslaStoreTokens();
  return true;
}

static bool teslaEnsureValidAccessToken(String* errorMessage) {
  teslaLoadTokens();

  if (gTeslaAuth.accessToken.isEmpty()) {
    return teslaRefreshAccessToken(errorMessage);
  }

  if (gTeslaAuth.expiresAt > 0) {
    time_t now = time(nullptr);
    if (now > 0 && (uint64_t)now >= gTeslaAuth.expiresAt) {
      return teslaRefreshAccessToken(errorMessage);
    }
  }

  return true;
}

static bool teslaHttpGet(const String& path, String* responseBody, String* errorMessage, bool allowRetry = true, int* statusCode = nullptr) {
  if (WiFi.status() != WL_CONNECTED) {
    if (errorMessage) {
      *errorMessage = "WiFi not connected";
    }
    return false;
  }

  if (strlen(TESLA_VEHICLE_ID) == 0) {
    if (errorMessage) {
      *errorMessage = "Tesla vehicle ID not configured";
    }
    return false;
  }

  if (!teslaEnsureValidAccessToken(errorMessage)) {
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure(); // TODO: Replace with proper root CA for production use.

  HTTPClient http;
  http.setTimeout(20000);
  String url = String(TESLA_API_BASE_URL) + path;
  if (!http.begin(client, url)) {
    if (errorMessage) {
      *errorMessage = "HTTP begin failed";
    }
    return false;
  }

  http.addHeader("Authorization", String("Bearer ") + gTeslaAuth.accessToken);

  int httpCode = http.GET();
  if (statusCode) {
    *statusCode = httpCode;
  }
  if (httpCode <= 0) {
    if (errorMessage) {
      *errorMessage = String("HTTP GET failed: ") + http.errorToString(httpCode);
    }
    http.end();
    return false;
  }

  if (httpCode == HTTP_CODE_UNAUTHORIZED && allowRetry) {
    http.end();
    if (teslaRefreshAccessToken(errorMessage)) {
      return teslaHttpGet(path, responseBody, errorMessage, false, statusCode);
    }
    return false;
  }

  if (httpCode != HTTP_CODE_OK) {
    if (errorMessage) {
      *errorMessage = String("HTTP status ") + httpCode + ": " + http.getString();
    }
    http.end();
    return false;
  }

  if (responseBody) {
    *responseBody = http.getString();
  }
  http.end();
  return true;
}

static bool teslaHttpPost(const String& path, const String& body, String* responseBody, String* errorMessage, bool allowRetry = true, int* statusCode = nullptr) {
  if (WiFi.status() != WL_CONNECTED) {
    if (errorMessage) {
      *errorMessage = "WiFi not connected";
    }
    return false;
  }

  if (strlen(TESLA_VEHICLE_ID) == 0) {
    if (errorMessage) {
      *errorMessage = "Tesla vehicle ID not configured";
    }
    return false;
  }

  if (!teslaEnsureValidAccessToken(errorMessage)) {
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure(); // TODO: Replace with proper root CA for production use.

  HTTPClient http;
  http.setTimeout(20000);
  String url = String(TESLA_API_BASE_URL) + path;
  if (!http.begin(client, url)) {
    if (errorMessage) {
      *errorMessage = "HTTP begin failed";
    }
    return false;
  }

  http.addHeader("Authorization", String("Bearer ") + gTeslaAuth.accessToken);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(body);
  if (statusCode) {
    *statusCode = httpCode;
  }
  if (httpCode <= 0) {
    if (errorMessage) {
      *errorMessage = String("HTTP POST failed: ") + http.errorToString(httpCode);
    }
    http.end();
    return false;
  }

  if (httpCode == HTTP_CODE_UNAUTHORIZED && allowRetry) {
    http.end();
    if (teslaRefreshAccessToken(errorMessage)) {
      return teslaHttpPost(path, body, responseBody, errorMessage, false, statusCode);
    }
    return false;
  }

  if (httpCode != HTTP_CODE_OK) {
    if (errorMessage) {
      *errorMessage = String("HTTP status ") + httpCode + ": " + http.getString();
    }
    http.end();
    return false;
  }

  if (responseBody) {
    *responseBody = http.getString();
  }
  http.end();
  return true;
}

static bool teslaWakeUp(String* errorMessage) {
  String response;
  String endpoint = String("/vehicles/") + TESLA_VEHICLE_ID + "/wake_up";
  return teslaHttpPost(endpoint, "{}", &response, errorMessage);
}

static bool teslaIsVehicleOnline(const String& json) {
  JsonDocument filter;
  filter["response"]["state"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json, DeserializationOption::Filter(filter));
  if (err) {
    return false;
  }

  const char* state = doc["response"]["state"] | "";
  return String(state) == "online";
}

static void teslaWaitForOnline(int maxAttempts = 6, uint32_t delayMs = 2000) {
  String response;
  String endpoint = String("/vehicles/") + TESLA_VEHICLE_ID;
  for (int attempt = 0; attempt < maxAttempts; ++attempt) {
    if (teslaHttpGet(endpoint, &response, nullptr)) {
      if (teslaIsVehicleOnline(response)) {
        return;
      }
    }
    delay(delayMs);
  }
}

static bool teslaHttpGetWithWake(const String& path, String* responseBody, String* errorMessage) {
  int statusCode = 0;
  if (teslaHttpGet(path, responseBody, errorMessage, true, &statusCode)) {
    return true;
  }

  bool isOffline = statusCode == HTTP_CODE_REQUEST_TIMEOUT;
  if (!isOffline && errorMessage) {
    isOffline = errorMessage->indexOf("vehicle unavailable") >= 0;
  }
  if (!isOffline) {
    return false;
  }

  if (!teslaWakeUp(errorMessage)) {
    return false;
  }

  teslaWaitForOnline();

  for (int attempt = 0; attempt < 3; ++attempt) {
    delay(2000);
    if (teslaHttpGet(path, responseBody, errorMessage, true, &statusCode)) {
      return true;
    }
  }
  return false;
}

static bool teslaFetchChargeStateWithRetry(TeslaTelemetry* telemetry, bool* hasRange, bool* hasBatteryLevel) {
  String chargeEndpoint = String("/vehicles/") + TESLA_VEHICLE_ID + "/data_request/charge_state";
  for (int attempt = 0; attempt < 3; ++attempt) {
    String chargeResponse;
    String chargeError;
    if (teslaHttpGetWithWake(chargeEndpoint, &chargeResponse, &chargeError)) {
      if (!chargeResponse.isEmpty() && teslaParseChargeState(chargeResponse, telemetry, hasRange, hasBatteryLevel)) {
        if ((hasRange && *hasRange) || (hasBatteryLevel && *hasBatteryLevel)) {
          return true;
        }
      }
    }
    (void)chargeResponse;
    (void)chargeError;
    delay(2000);
  }
  return false;
}

static bool teslaFetchVehicleStateWithRetry(TeslaTelemetry* telemetry, bool* hasOdometer) {
  String vehicleEndpoint = String("/vehicles/") + TESLA_VEHICLE_ID + "/data_request/vehicle_state";
  for (int attempt = 0; attempt < 3; ++attempt) {
    String vehicleResponse;
    String vehicleError;
    if (teslaHttpGetWithWake(vehicleEndpoint, &vehicleResponse, &vehicleError)) {
      if (!vehicleResponse.isEmpty() && teslaParseVehicleState(vehicleResponse, telemetry, hasOdometer)) {
        if (hasOdometer && *hasOdometer) {
          return true;
        }
      }
    }
    (void)vehicleResponse;
    (void)vehicleError;
    delay(2000);
  }
  return false;
}

static bool teslaParseVehicleData(const String& json, TeslaTelemetry* telemetry, String* errorMessage,
                                  bool* hasLocation = nullptr, bool* hasRange = nullptr,
                                  bool* hasOdometer = nullptr, bool* hasBatteryLevel = nullptr) {
  JsonDocument filter;
  filter["response"]["charge_state"]["est_battery_range"] = true;
  filter["response"]["charge_state"]["battery_range"] = true;
  filter["response"]["charge_state"]["ideal_battery_range"] = true;
  filter["response"]["charge_state"]["battery_level"] = true;
  filter["response"]["vehicle_state"]["odometer"] = true;
  filter["response"]["drive_state"]["latitude"] = true;
  filter["response"]["drive_state"]["longitude"] = true;
  filter["response"]["location_data"]["latitude"] = true;
  filter["response"]["location_data"]["longitude"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json, DeserializationOption::Filter(filter));
  if (err) {
    if (errorMessage) {
      *errorMessage = String("vehicle_data JSON parse failed: ") + err.c_str();
    }
    return false;
  }

  JsonVariant range = doc["response"]["charge_state"]["est_battery_range"];
  if (range.isNull()) {
    range = doc["response"]["charge_state"]["battery_range"];
  }
  if (range.isNull()) {
    range = doc["response"]["charge_state"]["ideal_battery_range"];
  }
  JsonVariant level = doc["response"]["charge_state"]["battery_level"];
  JsonVariant odometer = doc["response"]["vehicle_state"]["odometer"];
  JsonVariant lat = doc["response"]["drive_state"]["latitude"];
  JsonVariant lon = doc["response"]["drive_state"]["longitude"];
  if (lat.isNull() || lon.isNull()) {
    lat = doc["response"]["location_data"]["latitude"];
    lon = doc["response"]["location_data"]["longitude"];
  }

  bool foundLocation = !lat.isNull() && !lon.isNull();
  if (foundLocation) {
    telemetry->latitude = lat.as<double>();
    telemetry->longitude = lon.as<double>();
  }
  if (hasLocation) {
    *hasLocation = foundLocation;
  }
  if (hasRange) {
    *hasRange = !range.isNull();
  }
  if (hasOdometer) {
    *hasOdometer = !odometer.isNull();
  }
  if (hasBatteryLevel) {
    *hasBatteryLevel = !level.isNull();
  }

  if (!range.isNull()) {
    telemetry->estimatedBatteryRangeMiles = range.as<float>();
  }
  if (!level.isNull()) {
    telemetry->batteryLevelPercent = level.as<float>();
  }
  if (!odometer.isNull()) {
    telemetry->odometerMiles = odometer.as<float>();
  }
  return true;
}

static bool teslaParseChargeState(const String& json, TeslaTelemetry* telemetry, bool* hasRange, bool* hasBatteryLevel) {
  JsonDocument filter;
  filter["response"]["est_battery_range"] = true;
  filter["response"]["battery_range"] = true;
  filter["response"]["ideal_battery_range"] = true;
  filter["response"]["battery_level"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json, DeserializationOption::Filter(filter));
  if (err) {
    return false;
  }

  JsonVariant range = doc["response"]["est_battery_range"];
  if (range.isNull()) {
    range = doc["response"]["battery_range"];
  }
  if (range.isNull()) {
    range = doc["response"]["ideal_battery_range"];
  }
  JsonVariant level = doc["response"]["battery_level"];
  if (!range.isNull()) {
    telemetry->estimatedBatteryRangeMiles = range.as<float>();
  }
  if (!level.isNull()) {
    telemetry->batteryLevelPercent = level.as<float>();
  }
  if (hasRange) {
    *hasRange = !range.isNull();
  }
  if (hasBatteryLevel) {
    *hasBatteryLevel = !level.isNull();
  }
  return true;
}

static bool teslaParseVehicleState(const String& json, TeslaTelemetry* telemetry, bool* hasOdometer) {
  JsonDocument filter;
  filter["response"]["odometer"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json, DeserializationOption::Filter(filter));
  if (err) {
    return false;
  }

  JsonVariant odometer = doc["response"]["odometer"];
  if (!odometer.isNull()) {
    telemetry->odometerMiles = odometer.as<float>();
  }
  if (hasOdometer) {
    *hasOdometer = !odometer.isNull();
  }
  return true;
}

static bool teslaParseDriveState(const String& json, TeslaTelemetry* telemetry, String* errorMessage) {
  JsonDocument filter;
  filter["response"]["latitude"] = true;
  filter["response"]["longitude"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json, DeserializationOption::Filter(filter));
  if (err) {
    if (errorMessage) {
      *errorMessage = String("drive_state JSON parse failed: ") + err.c_str();
    }
    return false;
  }

  JsonVariant lat = doc["response"]["latitude"];
  JsonVariant lon = doc["response"]["longitude"];
  if (lat.isNull() || lon.isNull()) {
    if (errorMessage) {
      *errorMessage = "latitude or longitude missing in response";
    }
    return false;
  }
  telemetry->latitude = lat.as<double>();
  telemetry->longitude = lon.as<double>();
  return true;
}

static void teslaFetchLocationFromVehicleData(TeslaTelemetry* telemetry, bool* hasLocation) {
  String locResponse;
  String locEndpoint = String("/vehicles/") + TESLA_VEHICLE_ID + "/vehicle_data?endpoints=location_data;drive_state";
  if (teslaHttpGetWithWake(locEndpoint, &locResponse, nullptr)) {
    bool tempHasLocation = false;
    teslaParseVehicleData(locResponse, telemetry, nullptr, &tempHasLocation, nullptr, nullptr, nullptr);
    if (hasLocation) {
      *hasLocation = tempHasLocation;
    }
  }
}

bool teslaGetTelemetry(TeslaTelemetry* outTelemetry, String* errorMessage) {
  if (!outTelemetry) {
    if (errorMessage) {
      *errorMessage = "Telemetry output pointer is null";
    }
    return false;
  }

  TeslaTelemetry temp{};

  String response;
  String endpoint = String("/vehicles/") + TESLA_VEHICLE_ID + "/vehicle_data";
  int statusCode = 0;
  if (!teslaHttpGet(endpoint, &response, errorMessage, true, &statusCode)) {
    bool isOffline = statusCode == HTTP_CODE_REQUEST_TIMEOUT;
    if (!isOffline && errorMessage) {
      isOffline = errorMessage->indexOf("vehicle unavailable") >= 0;
    }

    if (isOffline) {
      if (!teslaWakeUp(errorMessage)) {
        return false;
      }

      bool success = false;
      for (int attempt = 0; attempt < 3; ++attempt) {
        delay(2000);
        bool hasLocation = false;
        bool hasRange = false;
        bool hasOdometer = false;
        bool hasBatteryLevel = false;
        if (teslaHttpGet(endpoint, &response, errorMessage, true, &statusCode) &&
            teslaParseVehicleData(response, &temp, errorMessage, &hasLocation, &hasRange, &hasOdometer, &hasBatteryLevel)) {
          if (!hasRange || !hasBatteryLevel || !hasOdometer) {
            teslaWakeUp(nullptr);
            teslaWaitForOnline();
            String retryResponse;
            if (teslaHttpGet(endpoint, &retryResponse, nullptr, true, &statusCode)) {
              teslaParseVehicleData(retryResponse, &temp, nullptr, &hasLocation, &hasRange, &hasOdometer, &hasBatteryLevel);
            }
          }
          if (!hasLocation) {
            teslaFetchLocationFromVehicleData(&temp, &hasLocation);
          }
          success = true;
          break;
        }
      }

      if (!success) {
        return false;
      }
    } else {
      return false;
    }
  } else {
    bool hasLocation = false;
    bool hasRange = false;
    bool hasOdometer = false;
    bool hasBatteryLevel = false;
    if (!teslaParseVehicleData(response, &temp, errorMessage, &hasLocation, &hasRange, &hasOdometer, &hasBatteryLevel)) {
      return false;
    }
    if (!hasRange || !hasBatteryLevel || !hasOdometer) {
      String retryResponse;
      if (teslaHttpGetWithWake(endpoint, &retryResponse, nullptr)) {
        teslaParseVehicleData(retryResponse, &temp, nullptr, &hasLocation, &hasRange, &hasOdometer, &hasBatteryLevel);
      }
    }
    if (!hasLocation) {
      teslaFetchLocationFromVehicleData(&temp, &hasLocation);
    }
  }

  temp.isValid = true;
  *outTelemetry = temp;
  return true;
}