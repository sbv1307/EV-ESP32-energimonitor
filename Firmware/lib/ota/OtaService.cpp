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
      Serial.println("OTA Start updating " + type);
      #ifdef DEBUG
                                                Serial.println("Network Task stopped for OTA update.");
                                                #endif
    /*
    Detach interrupts or tasks using WiFi or SPIFFS here
    */                                            
    })
    .onEnd([]() {
      Serial.println("\nOTA End");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("OTA Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
        Serial.println("Auth Failed");
      } else if (error == OTA_BEGIN_ERROR) {
        Serial.println("Begin Failed");
      } else if (error == OTA_CONNECT_ERROR) {
        Serial.println("Connect Failed");
      } else if (error == OTA_RECEIVE_ERROR) {
        Serial.println("Receive Failed");
      } else if (error == OTA_END_ERROR) {
        Serial.println("End Failed");
      }
    });

  ArduinoOTA.begin();
  
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
