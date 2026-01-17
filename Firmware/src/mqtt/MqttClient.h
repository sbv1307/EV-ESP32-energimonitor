
#pragma once

#include <Arduino.h>
#include <stdbool.h>
#include "MqttMessage.h"
#include "../globals/globals.h"

const String MQTT_PREFIX                    = "ev-e-monitor/";      // include tailing '/' in prefix!
const String MQTT_DEVICE_NAME               = "esp32-device_";      // MQTT device name prefix Incl. trailing underscore
const String MQTT_SKETCH_VERSION            = "/sketch_version";    // MQTT topic suffix for sketch version, include leading '/'
const String MQTT_CLIENT                    = "ev-energy-monitor_"; // MQTT client prefix. Include trailing underscore
const String MQTT_ONLINE                    = "/online";            // MQTT topic suffix for online status. Include leading '/'

const bool    RETAINED                      = true;                 // MQTT retained flag
// MQTT Subscription topics
const String MQTT_SUFFIX_TOTAL              = "/set_total";

/*
 * ##################################################################################################
 * ##################################################################################################
 * ##################################################################################################
 * ##########################   f u n c t i o n    d e c l a r a t i o n s  #########################
 * ##################################################################################################
 * ##################################################################################################
 * ##################################################################################################
 */

void publish_sketch_version(TaskParams_t* params);
void initializeMQTTGlobals();
bool mqttEnqueuePublish(const char* topic, const char* payload, bool retain);
void mqttInit( TaskParams_t* params );
void mqttLoop( TaskParams_t* params );
