#pragma once
/*
extern int systemState;
*/
#include <Arduino.h>

#define ssidNameSpaceKey "ssid"
#define passNameSpaceKey "pass"
#define mqttIPNameSpaceKey "mqttIP"
#define mqttPortNameSpaceKey "mqttPort"
#define mqttUserNameSpaceKey "mqttUser" 
#define mqttPasswordNameSpaceKey "mqttPass"

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