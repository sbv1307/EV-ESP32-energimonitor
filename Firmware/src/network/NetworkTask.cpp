#define DEBUG

#include <Arduino.h>

#include "NetworkTask.h"
#include "../ota/OtaService.h"
#include "../mqtt/MqttClient.h"

static TaskHandle_t networkTaskHandle = nullptr;

static void networkTask(void* pvParameters) {
  otaInit();
  mqttInit( (TaskParams_t*)pvParameters );

                                                    #ifdef DEBUG
                                                    Serial.println("Network Task initializing...");
                                                    #endif
  for (;;) {
    otaHandle();
    mqttLoop();

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void startNetworkTask(TaskParams_t* params) {
  xTaskCreatePinnedToCore(
    networkTask,
    "NetworkTask",
    4096,
    params,
    1,
    &networkTaskHandle,
    0   // Core 0 (Wi-Fi)
  );
}

void stopNetworkTask() {
  if (networkTaskHandle != nullptr) {
    vTaskDelete(networkTaskHandle);
    networkTaskHandle = nullptr;
  }
}
