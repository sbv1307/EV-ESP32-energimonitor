//# define DEBUG
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
    .nvsNamespace   = savedNvsNamespace
  };
  pref.end();

                                    #ifdef DEBUG
                                      Serial.println("Initialized global parameters:");
                                      Serial.println("Global WiFi SSID: " + String(params->wifiSSID));
                                      Serial.println("Global WiFi Password: " + String(params->wifiPassword));   
                                      Serial.println("Global MQTT Broker IP: " + String(params->mqttBrokerIP));
                                      Serial.println("Global MQTT Broker Port: " + String(params->mqttBrokerPort));
                                      Serial.println("Global MQTT Username: " + String(params->mqttUsername));
                                      Serial.println("Global MQTT Password: " + String(params->mqttPassword));
                                      Serial.println("Global Sketch Version: " + String(params->sketchVersion));
                                      Serial.println("Global NVS Namespace: " + String(params->nvsNamespace));
                                      Serial.println("-----------------------------------------------------");
                                    #endif    
  
}
