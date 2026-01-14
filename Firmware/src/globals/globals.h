#pragma once
/*
extern int systemState;
*/

#define SSIDnameSpaceKey "ssid"
#define PASSnameSpaceKey "pass"
#define MQTTIPnameSpaceKey "mqttIP"
#define MQTTPortnameSpaceKey "mqttPort"
#define MQTTUsernameSpaceKey "mqttUser" 
#define MQTTPasswordSpaceKey "mqttPass"

// Task parameter structure
typedef struct {
    const char* sketchVersion;
    const char* nvsNamespace;
} TaskParams_t;
