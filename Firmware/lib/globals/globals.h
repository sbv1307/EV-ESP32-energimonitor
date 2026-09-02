#pragma once
/*
extern int systemState;
*/
#include <Arduino.h>
#include <freertos/FreeRTOS.h>

// Task parameter structure
typedef struct {
    const char* wifiSSID;
    const char* wifiPassword;
    const char* mqttBrokerIP;
    int         mqttBrokerPort;
    const char* mqttUsername;
    const char* mqttPassword;
    const char* sketchVersion;
    const char* nvsNamespace;
    unsigned long ptCorrection;      // Pulse Time Correction used to calibrate the calculated consumption.
    uint16_t    pulse_per_kWh;       // Number of pulses per kWh for the energy meter

} TaskParams_t;

void initializeGlobals( TaskParams_t* params );

extern volatile UBaseType_t gNetworkTaskStackHighWater;
extern volatile UBaseType_t gWifiConnTaskStackHighWater;
extern volatile UBaseType_t gPulseInputTaskStackHighWater;
extern volatile UBaseType_t gTeslaTaskStackHighWater; // TOBE REMOVED. Only used for testing ISR pulse counting with a task that generates pulses in a loop. Not needed for actual pulse counting from the energy meter, which is handled by an interrupt service routine (ISR) and the Pulse Input Task.
extern volatile UBaseType_t gConfigurationTaskStackHighWater;
extern volatile UBaseType_t gButtonPublishTaskStackHighWater;
extern volatile UBaseType_t gLedStatusTaskStackHighWater;
extern volatile UBaseType_t gLedChargeTaskStackHighWater;
extern volatile UBaseType_t gDirectResetTaskStackHighWater;
extern volatile UBaseType_t gOledUpdateTaskStackHighWater;

extern volatile size_t  gInitialFreeHeapSize;

// Task stack sizes (in words)
constexpr int NETWORK_TASK_STACK_SIZE = 4608; // MQTT + OTA + button command handling share this task; raised headroom to avoid edge-case stack pressure.
constexpr int TESLA_TELEMETRY_TASK_STACK_SIZE = 8938; // Observed target around 8938 words; keep larger margin for HTTP/TLS and payload formatting.
constexpr int CONFIGURATION_TASK_STACK_SIZE = 5854; // Observed target around 3724 words; one-shot MQTT discovery publish path.
constexpr int WIFI_CONNECTION_TASK_STACK_SIZE = 2688; // Raised from 2007 because observed target is around 2517 words.
constexpr int PULSE_INPUT_TASK_STACK_SIZE = 3072; // Raised for added cost-tracking/reset/NVS paths; continue monitoring watermark.
constexpr int BUTTON_PUBLISH_TASK_STACK_SIZE = 2304; // Raised from 2130 because observed target is around 2145 words.
constexpr int LED_TASK_STACK_SIZE = 900; // Shared by both the Status and Charge LED tasks.
constexpr int DIRECT_RESET_TASK_STACK_SIZE = 2048; // Highest-priority task; handles emergency NVS save on direct-reset GPIO trigger.
constexpr int OLED_UPDATE_TASK_STACK_SIZE = 1424; // Background OLED redraw/touch-wake task.

// Global variables for display update
extern bool gDisplayUpdateAvailable; // Flag to indicate if a display update is needed
extern bool gSmartChargingActivated; // Flag to indicate if smart charging is activated. Set based on received MQTT messages, can be used to adjust display or logic accordingly.
extern float gChargeEnergyKwh; // Energy charged in the current session in kWh, updated at the end of the session
extern char gChargingStartTime[6];
extern float gCurrentEnergyPrice;
extern float gEnergyPriceRef;
extern float gEnergyPriceLimit;

// MQTT connection status flag
extern bool gMqttConnected; // Flag to indicate MQTT connection status, set by the WiFi Connection Task and used by other tasks to determine if they can publish or need to wait for a connection. This can help prevent failed publish attempts when MQTT is not connected. Tasks that need to publish can check this flag before attempting to publish, and if it's false, they can either skip publishing or queue the data for later publishing when the connection is restored.
extern volatile bool gControlledPowerCycle;