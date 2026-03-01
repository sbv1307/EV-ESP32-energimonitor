//#define DEBUG
#include <ArduinoOTA.h>

#include "OtaService.h"
#include "../network/NetworkTask.h"

void otaInit() {

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
      mqttPause();  // Stop MQTT operations during OTA
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else {  // U_SPIFFS
        type = "filesystem";
      }
      publishMqttLogStatus(("OTA start: " + type).c_str(), false);
      
                                                #ifdef DEBUG
                                                Serial.println("Network Task stopped for OTA update.");
                                                #endif
    /*
    Detach interrupts or tasks using WiFi or SPIFFS here
    */                                            
    })
    .onEnd([]() {
      publishMqttLogStatus("OTA end", false);
      mqttResume();  // Resume MQTT after OTA completes
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      char msg[48] = {0};
      unsigned int pct = (total == 0) ? 0 : (progress / (total / 100));
      snprintf(msg, sizeof(msg), "OTA progress: %u%%", pct);
      publishMqttLogStatus(msg, false);
    })
    .onError([](ota_error_t error) {
      char msg[32] = {0};
      snprintf(msg, sizeof(msg), "OTA error: %u", (unsigned)error);
      publishMqttLogStatus(msg, false);
      mqttResume();  // Ensure MQTT resumes even on error
    });

  ArduinoOTA.begin();
  publishMqttLogStatus("OTA service initialized", false);
  
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
