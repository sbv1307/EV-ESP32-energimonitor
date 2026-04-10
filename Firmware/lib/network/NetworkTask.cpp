//#define DEBUG
#define STACK_WATERMARK

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include "NetworkTask.h"
#include "globals.h"
#include "OtaService.h"
#include "MqttClient.h"
#include "PushButtonTask.h"



static TaskHandle_t networkTaskHandle = nullptr;
static TaskHandle_t wifiConnectionTaskHandle = nullptr;
static constexpr BaseType_t kNetworkTaskCore = 1;

static bool isWiFiConnectionActive() {
  wl_status_t status = WiFi.status();
  return status == WL_CONNECTED || status == WL_IDLE_STATUS || wifiConnectionTaskHandle != nullptr;
}

static void configureTime() {
  configTzTime("CET-1CEST,M3.5.0/2,M10.5.0/3",
               "pool.ntp.org",
               "time.nist.gov",
               "time.google.com");
}

static void networkTask(void* pvParameters) {
  otaInit();
  mqttInit( (TaskParams_t*)pvParameters );

                                                    #ifdef DEBUG
                                                    Serial.println("NetworkTask: Network Task initializing...\n");
                                                    #endif
  
  for (;;) {
    otaHandle();
    mqttLoop((TaskParams_t*)pvParameters);
    processPushButtonCommands();

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

                                                    #ifdef DEBUG
                                                    // Debug: Print initialized parameters
                                                    Serial.println("wifiConnectionTask: WiFi SSID: " + String(params->wifiSSID) + "\n");  
                                                    #endif

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
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(params->wifiSSID, params->wifiPassword);

  uint8_t connectResult = WiFi.waitForConnectResult(10000);
  if (connectResult != WL_CONNECTED && WiFi.status() != WL_CONNECTED) {

                                                    #ifdef DEBUG
                                                    Serial.println("\nWifiConnectionTask: WiFi connection failed. waitForConnectResult: " + String(connectResult) + ", WiFi status: " + String(WiFi.status()) + "\n");
                                                    #endif
    // Connection failed, clean up and exit task
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

  configureTime();

  // Create the network task
  xTaskCreatePinnedToCore(
    networkTask,
    "NetworkTask",
    NETWORK_TASK_STACK_SIZE,
    params,
    1,
    &networkTaskHandle,
    kNetworkTaskCore
  );

                                                  #ifdef STACK_WATERMARK
                                                  gWifiConnTaskStackHighWater = uxTaskGetStackHighWaterMark(nullptr);
                                                  #endif

    // Delete this initialization task as it's no longer needed
  wifiConnectionTaskHandle = nullptr;
  vTaskDelete(NULL);
}

bool startNetworkTask(TaskParams_t* params) {

                                                  #ifdef DEBUG
                                                  // Debug: Print initialized parameters
                                                  Serial.println("\nstartNetworkTask: WiFi SSID: " + String(params->wifiSSID) + "\n");
                                                  #endif


  if (wifiConnectionTaskHandle != nullptr) {
    // WiFi connection task is already running
    return false;
  }

  if (isWiFiConnectionActive()) {
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
    kNetworkTaskCore
  );
  return true;
}

void stopNetworkTask() {
  if (networkTaskHandle != nullptr) {
    vTaskDelete(networkTaskHandle);
    networkTaskHandle = nullptr;
  }
}

bool isWifiReconnectNeeded() {
  wl_status_t status = WiFi.status();
  return !isWiFiConnectionActive() && status != WL_NO_SHIELD;
}
