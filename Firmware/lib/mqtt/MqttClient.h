#pragma once

#include <Arduino.h>

#include "globals.h"

constexpr char MQTT_PREFIX[]                    = "ev-e-charging/";     // include tailing '/' in prefix!
constexpr char MQTT_DEVICE_NAME[]               = "esp32-doit_";        // MQTT device name prefix Incl. trailing underscore
constexpr char MQTT_SKETCH_VERSION[]            = "/sketch_version";    // MQTT topic suffix for sketch version, include leading '/'
constexpr char MQTT_CLIENT[]                    = "ev-energy-charging_"; // MQTT client prefix. Include trailing underscore
constexpr char MQTT_HA_CARD_NAME[]              = "Energi - EV Charging"; // Name used for Home Assistant card. No special chars, no spaces
constexpr char MQTT_ONLINE[]                    = "/online";            // MQTT topic suffix for online status. Include leading '/'
constexpr char MQTT_LOG_SUFFIX[]                = "/log";               // MQTT topic suffix for log messages. Include leading '/'
constexpr char MQTT_LOG_STATUS_SUFFIX[]         = "/log/status";        // MQTT topic suffix for status logs. Include leading '/'
constexpr char MQTT_LOG_EMAIL_SUFFIX[]          = "/log/email";         // MQTT topic suffix for email-routed logs. Include leading '/'
constexpr char MQTT_SENSOR_ENERGY_ENTITYNAME[]  = "Subtotal";           // name dislayed in HA device. No special chars, no spaces
constexpr char MQTT_SENSOR_POWER_ENTITYNAME[]   = "Forbrug";            // name dislayed in HA device. No special chars, no spaces
constexpr char MQTT_NUMBER_ENERGY_ENTITYNAME[]  = "Total";              // name dislayed in HA device. No special chars, no spaces
constexpr char MQTT_DISCOVERY_PREFIX[]          = "homeassistant/";     // include tailing '/' in discovery prefix!
constexpr char MQTT_SUFFIX_STATE[]              = "state";              // MQTT topic suffix for state messages. OBS no leading '/'

/*  MQTT publication definitions
 *  These definitions are used when publishing MQTT messages.
*/
const bool    RETAINED                      = true;                 // MQTT retained flag
// MQTT Subscription topics
constexpr char MQTT_SUFFIX_SET[]            = "/set";               // MQTT topic suffix for set commands. Include leading '/'  




/*  None configurable MQTT definitions
 *  These definitions are all defined in 'HomeAssistand -> MQTT' and cannot be changed.
*/
constexpr char MQTT_SENSOR_COMPONENT[]      = "sensor";
constexpr char MQTT_NUMBER_COMPONENT[]      = "number";
constexpr char MQTT_ENERGY_DEVICECLASS[]    = "energy";
constexpr char MQTT_POWER_DEVICECLASS[]     = "power";

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
void mqttProcessRxQueue();
void mqttProcessDeferredQueue();
void mqttCallback(char*, byte*, unsigned int);
void mqttPause();
void mqttResume();

void publishMqttConfigurations();
void publishMqttEnergy(float, float, float);
bool publishMqttLog(const char* topicSuffix, const char* message, bool retain = false);
bool publishMqttLogStatus(const char* message, bool retain = false);
bool publishMqttLogEmail(const char* message, bool retain = false);

