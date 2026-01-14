
#pragma once

#include <Arduino.h>
#include <stdbool.h>
#include "MqttMessage.h"
#include "../globals/globals.h"

const String MQTT_clientID                  = "esp32-client"; // MQTT client ID prefix
const String  MQTT_ONLINE                   = "/online";      // MQTT topic suffix for online status
// MQTT Subscription topics
const String  MQTT_SUFFIX_TOTAL    = "/set_total";

void mqttInit( TaskParams_t* params );
void mqttLoop();
bool mqttIsConnected();
bool mqttEnqueuePublish(const char* topic, const char* payload, bool retain);
