#pragma once
#include <Arduino.h>



constexpr char SSID[] = "DIGIFIBRA-u4UQ";
constexpr char PASS[] = "EX4E2Dsy6DF3";
constexpr char MQTT_BROKER[] = "192.168.1.132";
//constexpr char SSID[] = "Steens Hotspot";
//constexpr char PASS[] = "ceres1!S";
//constexpr char MQTT_BROKER[] = "10.44.60.6";
constexpr int MQTT_PORT = 1883;
constexpr char MQTT_USER[] = "";
constexpr char MQTT_PASS[] = "";

// Tesla Owner API (see https://tesla-api.timdorr.com/)
// Provide a valid OAuth access token and target vehicle ID.
constexpr char TESLA_ACCESS_TOKEN[] = "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCIsImtpZCI6InJQMWd2TjJicTFnZFhHWFhhaTM4U0ItdGt2OCJ9.eyJpc3MiOiJodHRwczovL2F1dGgudGVzbGEuY29tL29hdXRoMi92MyIsImF6cCI6Im93bmVyYXBpIiwic3ViIjoiMTVjMzc0YjUtYzE2Yy00ZjkzLWI0YjctNDBiYzdhMTcxNzQ3IiwiYXVkIjpbImh0dHBzOi8vb3duZXItYXBpLnRlc2xhbW90b3JzLmNvbS8iLCJodHRwczovL2F1dGgudGVzbGEuY29tL29hdXRoMi92My91c2VyaW5mbyJdLCJzY3AiOlsib3BlbmlkIiwiZW1haWwiLCJvZmZsaW5lX2FjY2VzcyJdLCJhbXIiOltdLCJleHAiOjE3NzAyMTY5OTgsImlhdCI6MTc3MDE4ODE5OCwib3VfY29kZSI6IkVVIiwibG9jYWxlIjoiZGEtREsifQ.D1JWv0cFEfDN1oPlx8tsPIAkSfg8HZwjtLpb3pgCiy4mbxuFdIGtr1Q8AZ9wPcGNqCrewGnR9S_d0lFtRccJBvrYgKeuE0FVswdJVeWj0kbUOS_EJwVeRyZ2qbTfTC86MLvUuxikfglNl9zIy36D0nBCKKkbIzoHTgRAve8xVzqaEep5CUUQiBXXWXXYxFfCQ92GtYBUpH57sWFHxWNAXDdSb6npotq1AseFDrm6hQOzdr5RqgeOkbEZP7c2l-ldCufJN_naO0nb-8RjN6sVDUVIG-0xHWu0WwW0hoX4Nrlfqbf5NtFRTnf2puJyK4s8E2jZNYbtbifv695YE3DhLA";
constexpr char TESLA_VEHICLE_ID[]   = "1492930692876308"; // numeric string, e.g. "12345678901234567"
constexpr char TESLA_REFRESH_TOKEN[] = "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCIsImtpZCI6InJQMWd2TjJicTFnZFhHWFhhaTM4U0ItdGt2OCJ9.eyJpc3MiOiJodHRwczovL2F1dGgudGVzbGEuY29tL29hdXRoMi92MyIsInNjcCI6WyJvcGVuaWQiLCJvZmZsaW5lX2FjY2VzcyJdLCJhdWQiOiJodHRwczovL2F1dGgudGVzbGEuY29tL29hdXRoMi92My90b2tlbiIsInN1YiI6IjE1YzM3NGI1LWMxNmMtNGY5My1iNGI3LTQwYmM3YTE3MTc0NyIsImRhdGEiOnsidiI6IjEiLCJhdWQiOiJodHRwczovL293bmVyLWFwaS50ZXNsYW1vdG9ycy5jb20vIiwic3ViIjoiMTVjMzc0YjUtYzE2Yy00ZjkzLWI0YjctNDBiYzdhMTcxNzQ3Iiwic2NwIjpbIm9wZW5pZCIsIm9mZmxpbmVfYWNjZXNzIl0sImF6cCI6Im93bmVyYXBpIiwiYW1yIjpbInB3ZCIsIm1mYSIsIm90cCJdLCJhdXRoX3RpbWUiOjE3NzAxODgxOTh9LCJpYXQiOjE3NzAxODgxOTh9.gaEdp7AGc-i0ivmbBqIpxGX72jKT3s22vtcIYotbbU1d2ReHoRGck_1MKCm3w_uYuQzKxT1JbTAkPDab5b9gZbd2uQCheJROmyEgQJMv6sjGL9OEybMCLMTDkDoWLhEM4TtBmJ9UMGCwtS_-Ip8HJg8-MgfI1YXSbdNfHYb2cesdx03PTfTgeRJ5un9BmqunLGR3UMVTiuiA3uTNhswV3FUwiwhmLGsFQrG4dU-GEjHuXdVW_g0bQT4cQhI953NuCwfXRlT76HGYjEuxQWc7J-kCZNSouTDXgh0nv6gDdKKGz8QXOeYN_bbZ05BAr7jUzMOfvI9dxbBaeIDt7YMJaA";
constexpr char TESLA_CLIENT_ID[] = "ownerapi";
constexpr char TESLA_CLIENT_SECRET[] = "";

// Google Sheets Web App endpoint (Apps Script Web App URL)
constexpr char TESLA_GSHEET_WEBAPP_URL_PREFIX[] = "https://script.google.com/macros/s/";
constexpr char TESLA_GSHEET_WEBAPP_DEPLOYMENT_ID[] = "AKfycbwHU_jIZmsCn4oAfjn2Ju6dhlJPAT2U9yQxi3d82yIyBu18W5Cm";
constexpr char TESLA_GSHEET_WEBAPP_URL_SUFFIX[] = "/exec";
constexpr char TESLA_GSHEET_PARAM_NAME[] = "TeslaLog";
