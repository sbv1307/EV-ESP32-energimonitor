//#define DEBUG
#define STACK_WATERMARK

#include <Arduino.h>
#include <WiFi.h>

#include "NetworkTask.h"
#include "globals.h"
#include "OtaService.h"
#include "MqttClient.h"



static TaskHandle_t networkTaskHandle = nullptr;
static TaskHandle_t wifiConnectionTaskHandle = nullptr;

static void networkTask(void* pvParameters) {
  otaInit();
  mqttInit( (TaskParams_t*)pvParameters );

                                                    #ifdef DEBUG
                                                    Serial.println("NetworkTask: Network Task initializing...\n");
                                                    #endif
  
  for (;;) {
    otaHandle();
    mqttLoop((TaskParams_t*)pvParameters);

    vTaskDelay(pdMS_TO_TICKS(10));

                                                    #ifdef STACK_WATERMARK
                                                    static uint32_t lastLog = 0;
                                                    if (millis() - lastLog > 5000) {
                                                      lastLog = millis();
                                                      gNetworkTaskStackHighWater = uxTaskGetStackHighWaterMark(nullptr);
                                                    }
                                                    #endif
  }
}

/* ###################################################################################################
 *                  W I F I   C O N N E C T I O N   T A S K
 * ###################################################################################################
 *  This task handles the WiFi connection process. It attempts to connect to the specified SSID and
 *  password, and upon successful connection, it starts the main network task.
 */
static void wifiConnectionTask(void* pvParameters) {
  TaskParams_t* params = (TaskParams_t*)pvParameters;

  // Debug: Print initialized parameters
  Serial.println("wifiConnectionTask: WiFi SSID: " + String(params->wifiSSID) + "\n");  


  
  stopNetworkTask();  // Ensure any existing network task is stopped

                                                    #ifdef DEBUG
                                                    Serial.println("wifiConnectionTask: Connecting to WiFi SSID: " + String(params->wifiSSID) + " ... \n");
                                                    #endif 

  // Disconnect if already connected
  if (WiFi.status() == WL_CONNECTED) {
    WiFi.disconnect();
    vTaskDelay(pdMS_TO_TICKS(1000));  // Give some time to disconnect
  }

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

                                                  #ifdef DEBUG
                                                      Serial.print(".");
                                                  #endif

    attempts++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {

                                                    #ifdef DEBUG
                                                    Serial.println("\nWifiConnectionTask: WiFi connection failed. WiFi status: " + String(WiFi.status()) + "\n");
                                                    #endif
    wifiConnectionTaskHandle = nullptr;
    vTaskDelete(NULL); // Delete this task
    return;
  }
  
  vTaskDelay(pdMS_TO_TICKS(1000));  // Give some time to settle WiFi connection

                                                  #ifdef DEBUG
                                                  Serial.println("\nwifiConnectionTask: WiFi connected successfully.");
                                                  Serial.print("\nWiFi connected. IP address: ");
                                                  Serial.println(WiFi.localIP());
                                                  #endif
                                                  
  
  // Create the network task
  xTaskCreatePinnedToCore(
    networkTask,
    "NetworkTask",
    NETWORK_TASK_STACK_SIZE,
    params,
    1,
    &networkTaskHandle,
    0   // Core 0 (Wi-Fi)
  );
  
                                                  #ifdef STACK_WATERMARK
                                                  gWifiConnTaskStackHighWater = uxTaskGetStackHighWaterMark(nullptr);
                                                  #endif

    // Delete this initialization task as it's no longer needed
  wifiConnectionTaskHandle = nullptr;
  vTaskDelete(NULL);
}

bool startNetworkTask(TaskParams_t* params) {

  // Debug: Print initialized parameters
  Serial.println("\nstartNetworkTask: WiFi SSID: " + String(params->wifiSSID) + "\n");  

  if (wifiConnectionTaskHandle != nullptr) {
    // WiFi connection task is already running
    return false;
  }
  // Create the WiFi connection task
  xTaskCreatePinnedToCore(
    wifiConnectionTask,
    "WiFiConnTask",
    WIFI_CONNECTION_TASK_STACK_SIZE,
    params,
    2,  // Higher priority to ensure WiFi connects first
    &wifiConnectionTaskHandle,  // Store the handle to avoid multiple instances
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
