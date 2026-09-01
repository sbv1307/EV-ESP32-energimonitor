#pragma once
#include <Arduino.h>

#ifndef CONFIG_PROFILE
#define CONFIG_PROFILE CONFIG_PROD
#endif

#ifndef GOOGLE_SHEETS_ENABLED
#define GOOGLE_SHEETS_ENABLED 1
#endif

#define CONFIG_PROD 1
#define CONFIG_TEST 2

// Example configuration for privateConfig.h
// Copy to privateConfig.h and replace placeholder values.
#if CONFIG_PROFILE == CONFIG_PROD
constexpr char SSID[] = "YOUR_PROD_WIFI_SSID";
constexpr char PASS[] = "YOUR_PROD_WIFI_PASSWORD";
constexpr char MQTT_BROKER[] = "YOUR_PROD_MQTT_BROKER_IP_OR_HOSTNAME";
#elif CONFIG_PROFILE == CONFIG_TEST
constexpr char SSID[] = "YOUR_TEST_WIFI_SSID";
constexpr char PASS[] = "YOUR_TEST_WIFI_PASSWORD";
constexpr char MQTT_BROKER[] = "YOUR_TEST_MQTT_BROKER_IP_OR_HOSTNAME";
#else
#error "Unsupported CONFIG_PROFILE. Use -D CONFIG_PROFILE=CONFIG_PROD or -D CONFIG_PROFILE=CONFIG_TEST"
#endif

constexpr int MQTT_PORT = YOUR_MQTT_BROKER_PORT; // e.g. 1883
constexpr char MQTT_USER[] = "";
constexpr char MQTT_PASS[] = "";

// Tesla Owner API (see https://tesla-api.timdorr.com/)
// Provide a valid OAuth access token and target vehicle id.
// IMPORTANT: Use the field named "id" from the Tesla API dump, not "vehicle_id".
constexpr char TESLA_ACCESS_TOKEN[] = "YOUR_TESLA_ACCESS_TOKEN";
constexpr char TESLA_OWNER_API_ID[] = "YOUR_TESLA_OWNER_API_ID"; // numeric string from field "id", e.g. "12345678901234567"
constexpr char TESLA_REFRESH_TOKEN[] = "YOUR_TESLA_REFRESH_TOKEN";
constexpr char TESLA_CLIENT_ID[] = "ownerapi";
constexpr char TESLA_CLIENT_SECRET[] = "";

// Tesla Auth Proxy (local service that performs token refresh over HTTP/2 + TLS 1.3)
// The proxy endpoint URL (e.g., "http://192.168.11.34:8787" or "http://your-pi-hostname:8787")
constexpr char TESLA_AUTH_PROXY_URL[] = "http://YOUR_PI_IP_OR_HOSTNAME:8787";
// Shared secret for HMAC-SHA256 request signing (same value as PROXY_SHARED_SECRET in proxy .env)
constexpr char TESLA_AUTH_PROXY_SHARED_SECRET[] = "YOUR_PROXY_SHARED_SECRET";

// Google Sheets Web App endpoint (Apps Script Web App URL)
constexpr char TESLA_GSHEET_WEBAPP_URL_PREFIX[] = "https://script.google.com/macros/s/";
constexpr char TESLA_GSHEET_WEBAPP_DEPLOYMENT_ID[] = "YOUR_DEPLOYMENT_ID";
constexpr char TESLA_GSHEET_WEBAPP_URL_SUFFIX[] = "/exec";
constexpr char TESLA_GSHEET_PARAM_NAME_LOG[] = "YOUR_GOOGLE_SHEET_NAME";
constexpr char TESLA_GSHEET_PARAM_NAME_DATA[] = "YOUR_CHARGING_SHEET_NAME";
