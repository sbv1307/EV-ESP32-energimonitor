#pragma once

#include <Arduino.h>

#include "globals.h"

const String MQTT_PREFIX                    = "ev-e-monitor/";      // include tailing '/' in prefix!
const String MQTT_DEVICE_NAME               = "esp32-device_";      // MQTT device name prefix Incl. trailing underscore
const String MQTT_SKETCH_VERSION            = "/sketch_version";    // MQTT topic suffix for sketch version, include leading '/'
const String MQTT_CLIENT                    = "ev-energy-monitor_"; // MQTT client prefix. Include trailing underscore
const String MQTT_ONLINE                    = "/online";            // MQTT topic suffix for online status. Include leading '/'
const String MQTT_LOG_SUFFIX                = "/log";               // MQTT topic suffix for log messages. Include leading '/'
const String MQTT_LOG_STATUS_SUFFIX         = "/log/status";        // MQTT topic suffix for status logs. Include leading '/'
const String MQTT_LOG_EMAIL_SUFFIX          = "/log/email";         // MQTT topic suffix for email-routed logs. Include leading '/'
const String  MQTT_SENSOR_ENERGY_ENTITYNAME  = "Subtotal";          // name dislayed in HA device. No special chars, no spaces
const String  MQTT_SENSOR_POWER_ENTITYNAME  = "Forbrug";            // name dislayed in HA device. No special chars, no spaces
const String  MQTT_NUMBER_ENERGY_ENTITYNAME  = "Total";             // name dislayed in HA device. No special chars, no spaces
const String  MQTT_DISCOVERY_PREFIX         = "homeassistant/";     // include tailing '/' in discovery prefix!
const String  MQTT_SUFFIX_STATE             = "state";              // MQTT topic suffix for state messages. OBS no leading '/'

/*  MQTT publication definitions
 *  These definitions are used when publishing MQTT messages.
*/
const bool    RETAINED                      = true;                 // MQTT retained flag
// MQTT Subscription topics
const String MQTT_SUFFIX_SET                = "/set";               // MQTT topic suffix for set commands. Include leading '/'  




/*  None configurable MQTT definitions
 *  These definitions are all defined in 'HomeAssistand -> MQTT' and cannot be changed.
*/
const String  MQTT_SENSOR_COMPONENT         = "sensor";
const String  MQTT_NUMBER_COMPONENT         = "number";
const String  MQTT_ENERGY_DEVICECLASS       = "energy";
const String  MQTT_POWER_DEVICECLASS        = "power";

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
void mqttCallback(char*, byte*, unsigned int);
void mqttPause();
void mqttResume();

void publishMqttConfigurations();
void publishMqttEnergy(float, float, float);
bool publishMqttLog(const char* topicSuffix, const char* message, bool retain = false);
bool publishMqttLogStatus(const char* message, bool retain = false);
bool publishMqttLogEmail(const char* message, bool retain = false);

