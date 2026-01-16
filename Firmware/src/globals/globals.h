#pragma once
/*
extern int systemState;
*/

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
} TaskParams_t;

void initializeGlobals( TaskParams_t* params );