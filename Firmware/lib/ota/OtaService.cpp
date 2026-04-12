//#define DEBUG
#include <ArduinoOTA.h>

#include "OtaService.h"
#include "MqttClient.h"
#include "oled_energy_display.h"
#include "oled_library.h"

namespace {
volatile bool sOtaInProgress = false;
bool sRestartOledUpdaterAfterOta = false;
}

bool isOtaInProgress() {
  return sOtaInProgress;
}

void otaInit() {
  OledEnergyDisplay::showMonitorLine("OTA init call");

                                                #ifdef DEBUG
                                                Serial.println("Init OTA service called.");
                                                #endif


  // Set OTA hostname (makes it easier to identify on network)
  ArduinoOTA.setHostname("ESP32-EnergyMonitor");
  
  // Set OTA port (default is 3232, but explicitly setting it)
  ArduinoOTA.setPort(3232);
  
  // Configure OTA callbacks
  ArduinoOTA
    .onStart([]() {
      sOtaInProgress = true;
      mqttPause();  // Stop MQTT operations during OTA
      sRestartOledUpdaterAfterOta = OledLibrary::isBackgroundUpdaterRunning();
      if (sRestartOledUpdaterAfterOta) {
        OledLibrary::stopBackgroundUpdater();
      }
      OledEnergyDisplay::setMonitorRenderingEnabled(false);
      OledEnergyDisplay::showMonitorLine("OTA start");
      
                                                #ifdef DEBUG
                                                Serial.println("Network Task stopped for OTA update.");
                                                #endif
    /*
    Detach interrupts or tasks using WiFi or SPIFFS here
    */                                            
    })
    .onEnd([]() {
      sOtaInProgress = false;
      mqttResume();  // Resume MQTT after OTA completes
      if (sRestartOledUpdaterAfterOta) {
        OledLibrary::startBackgroundUpdater();
        sRestartOledUpdaterAfterOta = false;
      }
      OledEnergyDisplay::setMonitorRenderingEnabled(true);
      OledEnergyDisplay::showMonitorLine("OTA done");

                                                #ifdef DEBUG
                                                Serial.println("OTA update completed. Network Task resumed.");
                                                #endif

    })
    /*
    .onProgress([](unsigned int progress, unsigned int total) {
      char msg[48] = {0};
      unsigned int pct = (total == 0) ? 0 : (progress / (total / 100));
      snprintf(msg, sizeof(msg), "OTA progress: %u%%", pct);
      publishMqttLogStatus(msg, false);
    })
    */
    .onError([](ota_error_t error) {
      sOtaInProgress = false;
      mqttResume();  // Ensure MQTT resumes even on error
      if (sRestartOledUpdaterAfterOta) {
        OledLibrary::startBackgroundUpdater();
        sRestartOledUpdaterAfterOta = false;
      }
      OledEnergyDisplay::setMonitorRenderingEnabled(true);
      char msg[32] = {0};
      snprintf(msg, sizeof(msg), "OTA error: %u", (unsigned)error);
      OledEnergyDisplay::showMonitorLine(msg);
      publishMqttLogStatus(msg, false);
    });

  ArduinoOTA.begin();
  publishMqttLogStatus("OTA service initialized", false);
  OledEnergyDisplay::showMonitorLine("OTA ready");
  OledEnergyDisplay::showMonitorLine("OTA host: ESP32-EM");
  OledEnergyDisplay::showMonitorLine("OTA IP: " + WiFi.localIP().toString());
  
                                                #ifdef DEBUG
                                                Serial.println("OTA service initialized");
                                                Serial.print("OTA hostname: ");
                                                Serial.println("ESP32-EnergyMonitor");
                                                Serial.print("OTA IP address: ");
                                                Serial.println(WiFi.localIP());
                                                #endif
}

void otaHandle() {
  ArduinoOTA.handle();
}
