//#define DEBUG
//#define HEADLESS_DEBUG
#include <Preferences.h>

#include "config.h"
#include "privateConfig.h"
#include "globals.h"
#include "build_timestamp.h"
#include "oled_energy_display.h"

// Global variables to track stack high water marks and initial free heap size
volatile UBaseType_t gNetworkTaskStackHighWater = 0;
volatile UBaseType_t gWifiConnTaskStackHighWater = 0;
volatile UBaseType_t gPulseInputTaskStackHighWater = 0;
volatile UBaseType_t gTeslaTaskStackHighWater = 0;
volatile UBaseType_t gConfigurationTaskStackHighWater = 0;
volatile UBaseType_t gButtonPublishTaskStackHighWater = 0;
volatile UBaseType_t gLedStatusTaskStackHighWater = 0;
volatile UBaseType_t gLedChargeTaskStackHighWater = 0;
volatile UBaseType_t gDirectResetTaskStackHighWater = 0;
volatile UBaseType_t gOledUpdateTaskStackHighWater = 0;
volatile size_t gInitialFreeHeapSize = 0;

// Initialize global variables for display update and smart charging status
bool gDisplayUpdateAvailable = true;
bool gSmartChargingActivated = false;
float gChargeEnergyKwh = 0.0f;
char gChargingStartTime[6] = {0};
float gCurrentEnergyPrice = 0.0f;
float gEnergyPriceRef = 0.0f;
float gEnergyPriceLimit = 0.0f;
bool gMqttConnected = false;
volatile bool gControlledPowerCycle = false;
volatile uint8_t gBootResetCause = BOOT_CAUSE_NONE;

void initializeGlobals( TaskParams_t* params ) {

  Preferences pref;
  pref.begin( CONFIG_NVS_NAMESPACE, false);
  
  static unsigned long ptCorrection = pref.getULong("ptCorrection", 0);
  static uint16_t pulse_per_kWh = pref.getUShort("pulse_per_kWh", 100);
  pref.end();

  Preferences countPref;
  countPref.begin(COUNT_NVS_NAMESPACE, false);
  gControlledPowerCycle = countPref.getBool("controlled_pwr", false);
  countPref.putBool("controlled_pwr", false);
  // Read-and-clear the boot cause so it is reported exactly once, by the boot that followed it.
  gBootResetCause = countPref.getUChar("reset_cause", BOOT_CAUSE_NONE);
  if (gBootResetCause != BOOT_CAUSE_NONE) {
    countPref.putUChar("reset_cause", BOOT_CAUSE_NONE);
  }
  countPref.end();

  static String sketchVersion = String(SKETCH_VERSION) + ". Build at: " + BUILD_TIMESTAMP;
  const char* savedSketchVersion = sketchVersion.c_str();
  const char* savedNvsNamespace = CONFIG_NVS_NAMESPACE;
  
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

                                    #ifdef HEADLESS_DEBUG
                                      OledEnergyDisplay::showMonitorLine("Globals init");
                                      OledEnergyDisplay::showMonitorLine("SSID: " + String(params->wifiSSID));
                                      OledEnergyDisplay::showMonitorLine("WiFi pw: " + String(params->wifiPassword));
                                      OledEnergyDisplay::showMonitorLine("MQT IP:" + String(params->mqttBrokerIP));
                                      OledEnergyDisplay::showMonitorLine("MQT port: " + String(params->mqttBrokerPort));
                                      OledEnergyDisplay::showMonitorLine("MQT user: " + String(params->mqttUsername));
                                      OledEnergyDisplay::showMonitorLine("MQT pw: " + String(params->mqttPassword));
                                      OledEnergyDisplay::showMonitorLine("Ver: " + String(params->sketchVersion));
                                      OledEnergyDisplay::showMonitorLine("NVS: " + String(params->nvsNamespace));
                                      OledEnergyDisplay::showMonitorLine("PT corr: " + String(params->ptCorrection));
                                      OledEnergyDisplay::showMonitorLine("Pulses/kWh: " + String(params->pulse_per_kWh));
                                    #endif

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
