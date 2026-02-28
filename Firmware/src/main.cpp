//#define NONE_HEADLESS
//#define DEBUG
//#define STACK_WATERMARK // Enable stack watermarking debug output. Requires NONE_HEADLESS to be defined.


#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>

#include "config.h"
#include "privateConfig.h"
#include "globals.h"
#include "NetworkTask.h"
#include "PulseInputTask.h"
#include "TeslaSheets.h"
#include "ChargingSession.h"
#include "MqttClient.h"
#include "oled_library.h"

#include "PulseInputIsrTest.h" //TOBE REMOVED. Only used for testing ISR pulse counting with a task that generates pulses in a loop. Not needed for actual pulse counting from the energy meter, which is handled by an interrupt service routine (ISR) and the Pulse Input Task.

#ifdef NONE_HEADLESS
#include <wait_for_any_key.h>
#include "build_timestamp.h"
#endif


/*
 * #################################################################################################
 * #################################################################################################
 *                  V  A  R  I  A  B  L  E      D  E  F  I  N  A  I  T  O  N  S
 * #################################################################################################
 * #################################################################################################
 */
static TaskParams_t networkParams;

/*
 * ##################################################################################################
 * ##################################################################################################
 * ##################################################################################################
 * ##########################   f u n c t i o n    d e c l a r a t i o n s  #########################
 * ##################################################################################################
 * ##################################################################################################
 * ##################################################################################################

 Functions are generally now declared in their respective header files and only included here.
 Function definitions are in their respective .cpp files.
 
 .h and .cpp file structure: 
lib/
 │── networks
 │── │── NetworkTask.h
 │── │── NetworkTask.cpp  // WiFi and network related functions
 │── ota
 |-- │── OtaService.h
 |-- │── OtaService.cpp   // OTA update related functions
 */

 /***************************************************************************************************
  * Function to handle daily telemetry sending and subtotal reset logic. 
  * This function checks if it's time to send daily telemetry data to Google Sheets 
  * and if the day has changed to reset the subtotal. It also handles pending telemetry data
  *  if WiFi was not connected at the scheduled time.
  */
 
static void handleDailyTelemetry(TaskParams_t *networkParams,
                                 int &lastDay,
                                 uint32_t &nextCheckMs,
                                 bool &bootTelemetryToSend,
                                 bool &pendingTelemetryToSend,
                                 float &pendingEnergyKwh);

static unsigned long calculateNextDelayMs(unsigned long wifiCheckInterval,
                                          uint32_t nextCheckMs,
                                          unsigned long lastStackLog);

/*
 * ###################################################################################################
 * ###################################################################################################
 * ###################################################################################################
 * ###################   S E T U P      B e g i n     ################################################
 * ###################################################################################################
 * ###################################################################################################
 * ###################################################################################################
 */
void setup() {

                                                              #ifdef NONE_HEADLESS
                                                              wait_for_any_key( SKETCH_VERSION + String(". Build at: ") + String(BUILD_TIMESTAMP));
                                                              #endif

                                                              #ifdef STACK_WATERMARK
                                                              gInitialFreeHeapSize = xPortGetFreeHeapSize();
                                                              #endif

  initializeGlobals( &networkParams );

  OledLibrary::Settings oledSettings;
  oledSettings.showSplashOnBoot = true;
  oledSettings.splashText = SKETCH_VERSION;
  oledSettings.splashDurationMs = 5000;
  oledSettings.turnOffAfterSplash = true;
  OledLibrary::begin(oledSettings);
  OledLibrary::startBackgroundUpdater(20, 1424, 1, 1);

  /*
   * Start the Pulse Input Task before the Network Task to ensure that pulse counting starts immediately
   * and is not delayed by network connection attempts. The Network Task will run in parallel and handle 
   * WiFi connectivity and MQTT communication.
   * If the Network Task is started first, there could be a delay in starting the Pulse Input Task until 
   * after a successful WiFi connection, which could lead to missed pulses during startup.
  */
  startPulseInputTask( &networkParams );

  waitForPulseInputReady(0);
  if (PULSE_INPUT_GPIO >= 0) {
    while (!attachPulseInputInterrupt(PULSE_INPUT_GPIO, PULSE_INPUT_INTERRUPT_MODE)) {
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }
  
  startNetworkTask( &networkParams );

  initChargingSession();
}

/*
 * ###################################################################################################
 * ###################################################################################################
 * ###################################################################################################
 *                       L O O P     B e g i n
 * ###################################################################################################
 * ###################################################################################################
 * ###################################################################################################
 */
void loop() {
  static unsigned long lastWiFiCheck = 0;
  const unsigned long wifiCheckInterval = 5000; // Check every 5 seconds
  static unsigned long lastStackLog = 0;
  static int lastDay = -1;
  static uint32_t nextCheckMs = 0;
  static bool bootTelemetryToSend = true;
  static bool pendingTelemetryToSend = false;
  static float pendingEnergyKwh = 0.0f;
  float energyKwh = 0.0f;

  static bool lastChargingSessionCharging = isChargingSessionCharging();

  unsigned long currentMillis = millis();
  
  if (currentMillis - lastWiFiCheck >= wifiCheckInterval) {
    lastWiFiCheck = currentMillis;
    
    if (WiFi.status() != WL_CONNECTED) {
                                                                      #ifdef DEBUG
                                                                      Serial.println("Main: WiFi disconnected. Attempting to reconnect...");
                                                                      #endif
      startNetworkTask( &networkParams );
    }
  }

  // Ensure Pulse Count Task is running. If already running, this does nothing.
  startPulseInputTask( &networkParams );

  //TOBE REMOVED Start the Pulse Input ISR Test Task to simulate pulse inputs for testing. This will only start if not already running.
  startPulseInputIsrTestTask();

  mqttProcessRxQueue();
  mqttProcessDeferredQueue();

  handleDailyTelemetry(&networkParams,
                       lastDay,
                       nextCheckMs,
                       bootTelemetryToSend,
                       pendingTelemetryToSend,
                       pendingEnergyKwh);
                       
  handleChargingSession(&networkParams);

  if (gDisplayUpdateAvailable || (isChargingSessionCharging() != lastChargingSessionCharging)) {
    gDisplayUpdateAvailable = false;
    lastChargingSessionCharging = isChargingSessionCharging();
    if (getLatestEnergyKwh(&energyKwh)) {
      OledEnergyDisplay::showEnergy(energyKwh, isChargingSessionCharging(), gChargeEnergyKwh,
                                  gSmartChargingActivated, gChargingStartTime, gCurrentEnergyPrice, gEnergyPriceLimit);
    }
  } 

  /*
   * Preparation for including:
   OledEnergyDisplay::showEnergy(energyKwh, isChargingSessionCharging, chargeEnergyKwh,
                                smartChargingActivated, chargingStartTime, currentEnergyPrice,
                                energyPriceLimit);
  * Options to get these values:
   * energyKwh:                   float energyKwh = 0.0f;
                                  if (getLatestEnergyKwh(&energyKwh)
   * isChargingSessionCharging:   isChargingSessionCharging()
   * chargeEnergyKwh:             In buildTeslaDataPayload() ->  
   *                              float chargeEnergyKwh = energyKwh - gSnapshot.startEnergyKwh;
   * The following would need to be stored in globals and updated from the MQTT data handling 
   * code when new data is received.
   * MQTT topic for these values will be "MQTT_PREFIX + mqttDeviceNameWithMac + MQTT_SUFFIX_SET" 
   * with a JSON payload containing a key - value pair for each of these parameters. For example:
   * {"smartChg": true, "chgStartTime": "12:34", "currEPrice": 1.37, "ePriceLimit": 0.94}
   * 
   * smartChargingActivated:    (smartChg)       ev_smart_charging_smart_charging_activated
   * chargingStartTime:         (chgStartTime)   select.ev_smart_charging_charge_start_time
   * currentEnergyPrice:        (currEPrice)     sensor.ev_smart_charging_charging.attributes.current_price
   * energyPriceLimit:          (ePriceLimit)    number.ev_smart_charging_electricity_price_limit
   * 
   * smartCharging implementation:
   */

  unsigned long nextDelayMs = calculateNextDelayMs(wifiCheckInterval,
                                                   nextCheckMs,
                                                   lastStackLog);

  vTaskDelay(pdMS_TO_TICKS(max(1UL, nextDelayMs)));

                                                                      #ifdef STACK_WATERMARK
                                                                        unsigned long stackNow = millis();
                                                                        if (stackNow - lastStackLog >= 5000) {
                                                                          lastStackLog = stackNow;
                                                                          static uint32_t maxOptimalNetworkTaskStackSize = 0;
                                                                          static uint32_t maxOptimalWifiConnTaskStackSize = 0;
                                                                          static uint32_t maxOptimalPulseInputTaskStackSize = 0;

                                                                          if (gNetworkTaskStackHighWater > 0) {
                                                                            uint32_t usedStack = NETWORK_TASK_STACK_SIZE - gNetworkTaskStackHighWater;
                                                                            uint32_t optimalNetworkTaskStackSize = (usedStack * 5 + 3) / 4; // Multiply by 1.25
                                                                            bool significantDiff = abs((int)NETWORK_TASK_STACK_SIZE - (int)optimalNetworkTaskStackSize) > 100;
                                                                            if (significantDiff && optimalNetworkTaskStackSize > maxOptimalNetworkTaskStackSize) {
                                                                              maxOptimalNetworkTaskStackSize = optimalNetworkTaskStackSize;
                                                                              char logMsg[128] = {0};
                                                                              snprintf(logMsg,
                                                                                       sizeof(logMsg),
                                                                                       "Change networkTaskStackSize from: %u to: %u words",
                                                                                       (unsigned)NETWORK_TASK_STACK_SIZE,
                                                                                       (unsigned)optimalNetworkTaskStackSize);
                                                                              publishMqttLog("log/stack/network", logMsg, false);
                                                                            }
                                                                          }
                                                                          if (gWifiConnTaskStackHighWater > 0) {
                                                                            uint32_t usedStack = WIFI_CONNECTION_TASK_STACK_SIZE - gWifiConnTaskStackHighWater;
                                                                            uint32_t optimalWifiConnTaskStackSize = (usedStack * 5 + 3) / 4; // Multiply by 1.25
                                                                            bool significantDiff = abs((int)WIFI_CONNECTION_TASK_STACK_SIZE - (int)optimalWifiConnTaskStackSize) > 100;
                                                                            if (significantDiff && optimalWifiConnTaskStackSize > maxOptimalWifiConnTaskStackSize) {
                                                                              maxOptimalWifiConnTaskStackSize = optimalWifiConnTaskStackSize;
                                                                              char logMsg[128] = {0};
                                                                              snprintf(logMsg,
                                                                                       sizeof(logMsg),
                                                                                       "Change wifiConnectionTaskStackSize from: %u to: %u words",
                                                                                       (unsigned)WIFI_CONNECTION_TASK_STACK_SIZE,
                                                                                       (unsigned)optimalWifiConnTaskStackSize);
                                                                              publishMqttLog("log/stack/wifiConnection", logMsg, false);
                                                                            }
                                                                          }
                                                                          if (gPulseInputTaskStackHighWater > 0) {
                                                                            uint32_t usedStack = PULSE_INPUT_TASK_STACK_SIZE - gPulseInputTaskStackHighWater;
                                                                            uint32_t optimalPulseInputTaskStackSize = (usedStack * 5 + 3) / 4; // Multiply by 1.25
                                                                            bool significantDiff = abs((int)PULSE_INPUT_TASK_STACK_SIZE - (int)optimalPulseInputTaskStackSize) > 100;
                                                                            if (significantDiff && optimalPulseInputTaskStackSize > maxOptimalPulseInputTaskStackSize) {
                                                                              maxOptimalPulseInputTaskStackSize = optimalPulseInputTaskStackSize;
                                                                              char logMsg[128] = {0};
                                                                              snprintf(logMsg,
                                                                                       sizeof(logMsg),
                                                                                       "Change pulseInputTaskStackSize from: %u to: %u words",
                                                                                       (unsigned)PULSE_INPUT_TASK_STACK_SIZE,
                                                                                       (unsigned)optimalPulseInputTaskStackSize);
                                                                              publishMqttLog("log/stack/pulseInput", logMsg, false);
                                                                            }
                                                                          }
                                                                          /*
                                                                          
                                                                          {
                                                                            char logMsg[96] = {0};
                                                                            snprintf(logMsg,
                                                                                     sizeof(logMsg),
                                                                                     "Initial Free Heap: %u bytes",
                                                                                     (unsigned)gInitialFreeHeapSize);
                                                                            publishMqttLogStatus(logMsg, false);
                                                                          }
                                                                          {
                                                                            char logMsg[96] = {0};
                                                                            snprintf(logMsg,
                                                                                     sizeof(logMsg),
                                                                                     "Current Free Heap: %u bytes",
                                                                                     (unsigned)xPortGetFreeHeapSize());
                                                                            publishMqttLogStatus(logMsg, false);
                                                                          }
                                                                          */
                                                                        }
                                                                      #endif

}

/*********************************************************************************************************
 * #################################################################################################
 * #################################################################################################
 *                 F u n c t i o n    D e f i n i t i o n s
 * #################################################################################################
 * #################################################################################################
 * Functions are generally now defined in their respective .cpp files. 
 * Only functions that are very specific to the main loop and not used elsewhere are defined here.
 */

static void handleDailyTelemetry(TaskParams_t *networkParams,
                                 int &lastDay,
                                 uint32_t &nextCheckMs,
                                 bool &bootTelemetryToSend,
                                 bool &pendingTelemetryToSend,
                                 float &pendingEnergyKwh) {
  static uint32_t lastTimeFailLogMs = 0;

  if (bootTelemetryToSend && WiFi.status() == WL_CONNECTED) {
    float energyKwh = 0.0f;
    if (getLatestEnergyKwh(&energyKwh)) {
      if (enqueueTeslaTelemetryToGoogleSheets(networkParams, energyKwh)) {
        bootTelemetryToSend = false;
        publishMqttLog(MQTT_LOG_SUFFIX, "Boot telemetry queued", false);
      }
    }
  }

  if (pendingTelemetryToSend && WiFi.status() == WL_CONNECTED) {
    if (enqueueTeslaTelemetryToGoogleSheets(networkParams, pendingEnergyKwh)) {
      pendingTelemetryToSend = false;
      publishMqttLog(MQTT_LOG_SUFFIX, "Pending telemetry queued", false);
    }
  }

  if (millis() >= nextCheckMs) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      if (lastDay != -1 && timeinfo.tm_mday != lastDay) {
        float energyKwh = 0.0f;
        if (getLatestEnergyKwh(&energyKwh)) {
          if (WiFi.status() == WL_CONNECTED) {
            if (enqueueTeslaTelemetryToGoogleSheets(networkParams, energyKwh)) {
              publishMqttLog(MQTT_LOG_SUFFIX, "Daily telemetry queued", false);
            } else {
              pendingEnergyKwh = energyKwh;
              pendingTelemetryToSend = true;
              publishMqttLog(MQTT_LOG_SUFFIX, "Daily telemetry pending (queue busy)", false);
            }
          } else {
            pendingEnergyKwh = energyKwh;
            pendingTelemetryToSend = true;
            publishMqttLog(MQTT_LOG_SUFFIX, "Daily telemetry pending (WiFi offline)", false);
          }
        }
        requestSubtotalReset();
        publishMqttLog(MQTT_LOG_SUFFIX, "Day changed, subtotal reset requested", false);
      }
      lastDay = timeinfo.tm_mday;

      uint32_t secsUntilMidnight = (23 - timeinfo.tm_hour) * 3600 +
                                   (59 - timeinfo.tm_min) * 60 +
                                   (60 - timeinfo.tm_sec);

      uint32_t checkInterval = max(secsUntilMidnight / 2, 600U) * 1000;
      nextCheckMs = millis() + checkInterval;
    } else {
      uint32_t nowMs = millis();
      if (nowMs - lastTimeFailLogMs >= 300000U) {
        publishMqttLogStatus("getLocalTime failed; daily telemetry/reset deferred", false);
        lastTimeFailLogMs = nowMs;
      }
    }
  }
}

static unsigned long calculateNextDelayMs(unsigned long wifiCheckInterval,
                                          uint32_t nextCheckMs,
                                          unsigned long lastStackLog) {
  unsigned long nextDelayMs = wifiCheckInterval;
  unsigned long nowMs = millis();

  if (nextCheckMs > nowMs) {
    nextDelayMs = min(nextDelayMs, nextCheckMs - nowMs);
  } else {
    nextDelayMs = 0;
  }

  const unsigned long stackIntervalMs = 5000;
  if (nowMs - lastStackLog < stackIntervalMs) {
    nextDelayMs = min(nextDelayMs, stackIntervalMs - (nowMs - lastStackLog));
  } else {
    nextDelayMs = 0;
  }

  return nextDelayMs;
}
