# define DEBUG
#include <Preferences.h>

#include "globals.h"

void initializeGlobals( TaskParams_t* params ) {

  Preferences pref;
  pref.begin( params->nvsNamespace, false);
  
  // Store strings to prevent dangling pointers from temporary String objects
  static String ssid = pref.getString(ssidNameSpaceKey, "");
  static String password = pref.getString(passNameSpaceKey, "");
  static String mqttIP = pref.getString(mqttIPNameSpaceKey, "");
  static String mqttUser = pref.getString(mqttUserNameSpaceKey, "");
  static String mqttPass = pref.getString(mqttPasswordNameSpaceKey, "");
  static unsigned long ptCorrection = pref.getULong("ptCorrection", 0);
  static uint16_t pulse_per_kWh = pref.getUShort("pulse_per_kWh", 100);

  // Preserve the existing sketchVersion and nvsNamespace values
  const char* savedSketchVersion = params->sketchVersion;
  const char* savedNvsNamespace = params->nvsNamespace;
  
  *params = {
    .wifiSSID       = ssid.c_str(),
    .wifiPassword   = password.c_str(),
    .mqttBrokerIP   = mqttIP.c_str(),
    .mqttBrokerPort = pref.getString(mqttPortNameSpaceKey, "").toInt(),
    .mqttUsername   = mqttUser.c_str(),
    .mqttPassword   = mqttPass.c_str(),
    .sketchVersion  = savedSketchVersion,
    .nvsNamespace   = savedNvsNamespace,
    .ptCorrection   = ptCorrection,
    .pulse_per_kWh  = pulse_per_kWh
  };
  pref.end();

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
