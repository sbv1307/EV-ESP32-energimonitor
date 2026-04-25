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

extern volatile size_t  gInitialFreeHeapSize;

// Task stack sizes (in words)
constexpr int NETWORK_TASK_STACK_SIZE = 3849; // Optimal size: 3742 stack size for the task
constexpr int TESLA_TELEMETRY_TASK_STACK_SIZE = 8192; // Optimal size: 7880 stack size for the task
constexpr int CONFIGURATION_TASK_STACK_SIZE = 4835; // Optimal size: 3724 stack size for the task. This task is used for publishing MQTT configurations, which can involve building large JSON payloads, so it may require more stack than typical tasks. It's a one-shot task that runs at startup and after OTA updates to publish the device configuration to MQTT, and then deletes itself. The stack size can be adjusted based on observed high water marks during testing to ensure it has enough stack for the largest expected configuration payloads without being excessively large.
constexpr int WIFI_CONNECTION_TASK_STACK_SIZE = 2657; // Optimal size: 2517 stack size for the WiFi connection task. This task handles WiFi connectivity and MQTT communication, which can involve operations that require more stack, especially during MQTT reconnection attempts and publishing. The stack size can be adjusted based on observed high water marks during testing to ensure it has enough stack for these operations without being excessively large.
constexpr int PULSE_INPUT_TASK_STACK_SIZE = 2525; // Optimal size:    8KB stack size for the task
constexpr int BUTTON_PUBLISH_TASK_STACK_SIZE = 3048; // Optimal size: 2145 One-shot task stack size for MQTT publish triggered by button ISR queue.
// OledUpdateTaskStackSize is defined in oled_library.h since it's only used for the OLED update task, which is defined in that library.

// Global variables for display update
extern bool gDisplayUpdateAvailable; // Flag to indicate if a display update is needed
extern bool gSmartChargingActivated; // Flag to indicate if smart charging is activated. Set based on received MQTT messages, can be used to adjust display or logic accordingly.
extern float gChargeEnergyKwh; // Energy charged in the current session in kWh, updated at the end of the session
extern char gChargingStartTime[6];
extern float gEnergyPriceRef;
extern float gEnergyPriceLimit;

// MQTT connection status flag
extern bool gMqttConnected; // Flag to indicate MQTT connection status, set by the WiFi Connection Task and used by other tasks to determine if they can publish or need to wait for a connection. This can help prevent failed publish attempts when MQTT is not connected. Tasks that need to publish can check this flag before attempting to publish, and if it's false, they can either skip publishing or queue the data for later publishing when the connection is restored.