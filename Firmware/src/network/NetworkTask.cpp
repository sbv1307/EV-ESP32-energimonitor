#define DEBUG

#include <Arduino.h>
#include <WiFi.h>

#include "NetworkTask.h"
#include "../ota/OtaService.h"
#include "../mqtt/MqttClient.h"

static TaskHandle_t networkTaskHandle = nullptr;

static void networkTask(void* pvParameters) {
  otaInit();
  mqttInit( (TaskParams_t*)pvParameters );

                                                    #ifdef DEBUG
                                                    Serial.println("Network Task initializing...\n");
                                                    #endif
  
  for (;;) {
    otaHandle();
    mqttLoop((TaskParams_t*)pvParameters);

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

bool startNetworkTask(TaskParams_t* params) {
  // Connect to WiFi first
  WiFi.begin(params->wifiSSID, params->wifiPassword);
  
  int attempts = 0;
  const int maxAttempts = 20; // 10 seconds timeout (20 * 500ms)
  
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi connection failed.");
    return false;
  }
  
  delay(1000);  // Give some time to settle WiFi connection
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());
  
  // Create the network task
  xTaskCreatePinnedToCore(
    networkTask,
    "NetworkTask",
    4096,
    params,
    1,
    &networkTaskHandle,
    0   // Core 0 (Wi-Fi)
  );
  
  return true;
}

void stopNetworkTask() {
  if (networkTaskHandle != nullptr) {
    vTaskDelete(networkTaskHandle);
    networkTaskHandle = nullptr;
  }
}
