#define DEBUG

#include "OtaService.h"
#include "../network/NetworkTask.h"
#include <ArduinoOTA.h>

void otaInit() {
  ArduinoOTA
    .onStart([]() {
      Serial.println("OTA Start");
      stopNetworkTask();
                                                #ifdef DEBUG
                                                Serial.println("Network Task stopped for OTA update.");
                                                #endif
    /*
    Detach interrupts or tasks using WiFi or SPIFFS here
    */                                            
    })
    .onEnd([]() {
      Serial.println("OTA End");
    })
    .onError([](ota_error_t error) {
      Serial.printf("OTA Error[%u]\n", error);
    });

  ArduinoOTA.begin();
}

void otaHandle() {
  ArduinoOTA.handle();
}
