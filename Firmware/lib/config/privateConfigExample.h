#pragma once
#include <Arduino.h>

// Example configuration for privateConfig.h
// Copy to privateConfig.h and replace placeholder values.

constexpr char SSID[] = "YOUR_WIFI_SSID";
constexpr char PASS[] = "YOUR_WIFI_PASSWORD";
constexpr char MQTT_BROKER[] = "YOUR_MQTT_BROKER_IP_OR_HOSTNAME";
constexpr int MQTT_PORT = YOUR_MQTT_BROKER_PORT; // e.g. 1883
constexpr char MQTT_USER[] = "";
constexpr char MQTT_PASS[] = "";

// Tesla Owner API (see https://tesla-api.timdorr.com/)
// Provide a valid OAuth access token and target vehicle ID.
constexpr char TESLA_ACCESS_TOKEN[] = "YOUR_TESLA_ACCESS_TOKEN";
constexpr char TESLA_VEHICLE_ID[]   = "YOUR_TESLA_VEHICLE_ID"; // numeric string, e.g. "12345678901234567"
constexpr char TESLA_REFRESH_TOKEN[] = "YOUR_TESLA_REFRESH_TOKEN";
constexpr char TESLA_CLIENT_ID[] = "ownerapi";
constexpr char TESLA_CLIENT_SECRET[] = "";

// Google Sheets Web App endpoint (Apps Script Web App URL)
constexpr char TESLA_GSHEET_WEBAPP_URL_PREFIX[] = "https://script.google.com/macros/s/";
constexpr char TESLA_GSHEET_WEBAPP_DEPLOYMENT_ID[] = "YOUR_DEPLOYMENT_ID";
constexpr char TESLA_GSHEET_WEBAPP_URL_SUFFIX[] = "/exec";
constexpr char TESLA_GSHEET_PARAM_NAME[] = "YOUR_GOOGLE_SHEET_NAME";
constexpr char TESLA_GSHEET_PARAM_NAME_DATA[] = "YOUR_CHARGING_SHEET_NAME";
