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
extern volatile size_t  gInitialFreeHeapSize;

// Task stack sizes (in words)
constexpr int NETWORK_TASK_STACK_SIZE = 3529; // 16KB stack size for the task
constexpr int WIFI_CONNECTION_TASK_STACK_SIZE = 2505;
constexpr int PULSE_INPUT_TASK_STACK_SIZE = 4096; // 8KB stack size for the task
constexpr int PULSE_ISR_TEST_TASK_STACK_SIZE = 4096; // TOBE REMOVED. Only used for testing ISR pulse counting with a task that generates pulses in a loop. Not needed for actual pulse counting from the energy meter, which is handled by an interrupt service routine (ISR) and the Pulse Input Task.
// OledUpdateTaskStackSize is defined in oled_library.h since it's only used for the OLED update task, which is defined in that library.

// Global variables for display update
extern bool gDisplayUpdateAvailable; // Flag to indicate if a display update is needed
extern bool gSmartChargingActivated; // Flag to indicate if smart charging is activated. Set based on received MQTT messages, can be used to adjust display or logic accordingly.
extern float gChargeEnergyKwh; // Energy charged in the current session in kWh, updated at the end of the session
extern char gChargingStartTime[6];
extern float gCurrentEnergyPrice;
extern float gEnergyPriceLimit;