//#define DEBUG

#include <Arduino.h>
#include <WiFi.h>

#include "NetworkTask.h"
#include "../ota/OtaService.h"
#include "../mqtt/MqttClient.h"

static TaskHandle_t networkTaskHandle = nullptr;
static TaskHandle_t wifiConnectionTaskHandle = nullptr;

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

static void wifiConnectionTask(void* pvParameters) {
  TaskParams_t* params = (TaskParams_t*)pvParameters;
  
  stopNetworkTask();  // Ensure any existing network task is stopped

                                                    #ifdef DEBUG
                                                    Serial.print("Connecting to WiFi SSID: ");
                                                    Serial.println(params->wifiSSID);
                                                    #endif 

  // Disconnect if already connected
  WiFi.disconnect();
  // Connect to WiFi
  WiFi.begin(params->wifiSSID, params->wifiPassword);
  
  int attempts = 0;
  const int maxAttempts = 20; // 10 seconds timeout (20 * 500ms)
  
  /*
   * Key difference:
   * WiFi.status()               - Returns the current WiFi status immediately (non-blocking)
   * WiFi.waitForConnectResult() - Blocks and waits for the connection attempt to complete 
   *                               before returning.
   * The problem:
   * Using WiFi.waitForConnectResult() in the loop defeats the purpose of your manual timeout mechanism.
   * It will block for the entire connection attempt duration on each iteration, making your attempts
   * counter and delay(500) ineffective.
   */
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    vTaskDelay(pdMS_TO_TICKS(500));
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi connection failed.");
    wifiConnectionTaskHandle = nullptr;
    vTaskDelete(NULL); // Delete this task
    return;
  }
  
  vTaskDelay(pdMS_TO_TICKS(1000));  // Give some time to settle WiFi connection
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
  
  // Delete this initialization task as it's no longer needed
  wifiConnectionTaskHandle = nullptr;
  vTaskDelete(NULL);
}

bool startNetworkTask(TaskParams_t* params) {
  if (wifiConnectionTaskHandle != nullptr) {
    // WiFi connection task is already running
    return false;
  }
  // Create the WiFi connection task
  xTaskCreatePinnedToCore(
    wifiConnectionTask,
    "WiFiConnTask",
    4096,
    params,
    2,  // Higher priority to ensure WiFi connects first
    &wifiConnectionTaskHandle,  // Don't need to store the handle
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
