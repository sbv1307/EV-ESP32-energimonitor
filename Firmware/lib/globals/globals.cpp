//#define DEBUG
#include <Preferences.h>

#include "config.h"
#include "privateConfig.h"
#include "globals.h"
#include "build_timestamp.h"

// Global variables to track stack high water marks and initial free heap size
volatile UBaseType_t gNetworkTaskStackHighWater = 0;
volatile UBaseType_t gWifiConnTaskStackHighWater = 0;
volatile UBaseType_t gPulseInputTaskStackHighWater = 0;
volatile size_t gInitialFreeHeapSize = 0;

void initializeGlobals( TaskParams_t* params ) {

  Preferences pref;
  pref.begin( NVS_NAMESPACE, false);
  
  static unsigned long ptCorrection = pref.getULong("ptCorrection", 0);
  static uint16_t pulse_per_kWh = pref.getUShort("pulse_per_kWh", 100);
  pref.end();



  static String sketchVersion = String(SKETCH_VERSION) + ". Build at: " + BUILD_TIMESTAMP;
  const char* savedSketchVersion = sketchVersion.c_str();
  const char* savedNvsNamespace = NVS_NAMESPACE;
  
  *params = {
    .wifiSSID       = SSID,
    .wifiPassword   = PASS,
    .mqttBrokerIP   = MQTT_BROKER,
    .mqttBrokerPort = MQTT_PORT,
    .mqttUsername   = MQTT_USER,
    .mqttPassword   = MQTT_PASS,
    .sketchVersion  = savedSketchVersion,
    .nvsNamespace   = savedNvsNamespace,
    .ptCorrection   = ptCorrection,
    .pulse_per_kWh  = pulse_per_kWh
  };

                                    #ifdef DEBUG
                                      Serial.println("globals: Initialized global parameters:");
                                      Serial.println("globals: WiFi SSID: " + String(params->wifiSSID));
                                      Serial.println("globals: WiFi Password: " + String(params->wifiPassword));   
                                      Serial.println("globals: MQTT Broker IP: " + String(params->mqttBrokerIP));
                                      Serial.println("globals: MQTT Broker Port: " + String(params->mqttBrokerPort));
                                      Serial.println("globals: MQTT Username: " + String(params->mqttUsername));
                                      Serial.println("globals: MQTT Password: " + String(params->mqttPassword));
                                      Serial.println("globals: Sketch Version: " + String(params->sketchVersion));
                                      Serial.println("globals: NVS Namespace: " + String(params->nvsNamespace));
                                      Serial.println("globals: Pulse Time Correction: " + String(params->ptCorrection));
                                      Serial.println("globals: Pulses per kWh: " + String(params->pulse_per_kWh));
                                      Serial.println("-----------------------------------------------------");
                                    #endif    
  
}
