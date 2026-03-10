//#define NONE_HEADLESS
//#define DEBUG
//#define VERIFY_LOCAL_TIME
//#define BOOT_DIAGNOSTICS_LOGGING // Enable logging of boot diagnostics (reset reason, boot count, uptime) to MQTT. Requires WiFi connection and may delay the first telemetry if the MQTT broker is not reachable at startup.
#define STACK_WATERMARK // Enable stack watermarking debug output. Requires NONE_HEADLESS to be defined.

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

                                                          #ifdef BOOT_DIAGNOSTICS_LOGGING
                                                          #include <esp_system.h>
                                                          #endif

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

                                                                  #ifdef BOOT_DIAGNOSTICS_LOGGING
                                                                  RTC_DATA_ATTR static uint32_t sBootCount = 0;
                                                                  #endif

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
                                 int &lastDateKey,
                                 uint32_t &nextCheckMs,
                                 bool &bootTelemetryToSend,
                                 bool &pendingTelemetryToSend,
                                 float &pendingEnergyKwh);

                                                              #ifdef VERIFY_LOCAL_TIME
                                                              static void verifyLocalTimeHealth(); // TOBE REMOVED: Checks if local time is valid and logs the current time and epoch to MQTT for debugging.
                                                              #endif

static unsigned long calculateNextDelayMs(unsigned long wifiCheckInterval,
                                          uint32_t nextCheckMs,
                                          unsigned long lastStackLog);

                                                              #ifdef BOOT_DIAGNOSTICS_LOGGING
                                                              static const char* resetReasonToString(esp_reset_reason_t reason);
                                                              static void publishBootDiagnosticsOnce();
                                                              #endif

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

  /*
   * Start the Pulse Input Task before the Network Task to ensure that pulse counting starts immediately
   * and is not delayed by network connection attempts. The Network Task will run in parallel and handle 
   * WiFi connectivity and MQTT communication.
   * If the Network Task is started first, there could be a delay in starting the Pulse Input Task until 
   * after a successful WiFi connection, which could lead to missed pulses during startup.
  */
  startPulseInputTask( &networkParams );

  /*
   * Wait for the Pulse Input Task to be ready before proceeding. This ensures that pulse counting starts immediately
   * and is not delayed by other tasks. The Pulse Input Task must be ready before attaching the interrupt.
  */
  waitForPulseInputReady(0);
  if (PULSE_INPUT_GPIO >= 0) {
    while (!attachPulseInputInterrupt(PULSE_INPUT_GPIO, PULSE_INPUT_INTERRUPT_MODE)) {
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }
  
  /*
   * Initialize the OLED display with the specified settings. The display will show a splash screen on boot 
   * with the sketch version, and then turn off after the splash duration. The background updater is started 
   * to allow for asynchronous updates to the display when new energy data is available or when the charging 
   * state changes.
   * Initializing the display before starting the Network Task allows for immediate feedback on the device 
   * status (e.g., showing the splash screen) without waiting for network connectivity, which can enhance 
   * the user experience during startup.
  */
  OledLibrary::Settings oledSettings;
  oledSettings.showSplashOnBoot = true;
  oledSettings.splashText = SKETCH_VERSION;
  oledSettings.splashDurationMs = 5000;
  oledSettings.turnOffAfterSplash = true;
  OledLibrary::begin(oledSettings);
  OledLibrary::startBackgroundUpdater(20, 1424, 1, 1);

  startNetworkTask( &networkParams );

  initChargingSession();

                                                            #ifdef BOOT_DIAGNOSTICS_LOGGING
                                                            sBootCount++;
                                                            #endif
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
  static int lastDateKey = -1;
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

                                                                    #ifdef BOOT_DIAGNOSTICS_LOGGING
                                                                    publishBootDiagnosticsOnce();
                                                                    #endif

                                                                    #ifdef VERIFY_LOCAL_TIME
                                                                    verifyLocalTimeHealth(); // TOBE REMOVED: Checks if local time is valid and logs the current time and epoch to MQTT for debugging.
                                                                    #endif

  handleDailyTelemetry(&networkParams,
                       lastDateKey,
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
                                  gSmartChargingActivated, gChargingStartTime, gCurrentEnergyRef, gEnergyPriceLimit);
    }
  } 

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
                                                                          static uint32_t maxOptimalTeslaTaskStackSize = 0;
                                                                          static uint32_t maxOptimalConfigurationTaskStackSize = 0;
                                                                          static UBaseType_t minLoopTaskStackHighWater = 0;

                                                                          UBaseType_t loopTaskStackHighWater = uxTaskGetStackHighWaterMark(nullptr);
                                                                          if (loopTaskStackHighWater > 0 &&
                                                                              (minLoopTaskStackHighWater == 0 || loopTaskStackHighWater < minLoopTaskStackHighWater)) {
                                                                            minLoopTaskStackHighWater = loopTaskStackHighWater;
                                                                            char logMsg[128] = {0};
                                                                            snprintf(logMsg,
                                                                                     sizeof(logMsg),
                                                                                     "loop() min free stack watermark: %u words",
                                                                                     (unsigned)minLoopTaskStackHighWater);
                                                                            publishMqttLog("log/stack/loop", logMsg, false);
                                                                          }

                                                                          if (gNetworkTaskStackHighWater > 0) {
                                                                            uint32_t usedStack = NETWORK_TASK_STACK_SIZE - gNetworkTaskStackHighWater;
                                                                            uint32_t optimalNetworkTaskStackSize = (usedStack * 5 + 3) / 4; // Multiply by 1.25
                                                                            bool significantDiff = abs((int)NETWORK_TASK_STACK_SIZE - (int)optimalNetworkTaskStackSize) > 100;
                                                                            if (significantDiff && optimalNetworkTaskStackSize > maxOptimalNetworkTaskStackSize) {
                                                                              maxOptimalNetworkTaskStackSize = optimalNetworkTaskStackSize;
                                                                              char logMsg[128] = {0};
                                                                              snprintf(logMsg,
                                                                                       sizeof(logMsg),
                                                                                       "Change NETWORK_TASK_STACK_SIZE from: %u to: %u words",
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
                                                                                       "Change WIFI_CONNECTION_TASK_STACK_SIZE from: %u to: %u words",
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
                                                                                       "Change PULSE_INPUT_TASK_STACK_SIZE from: %u to: %u words",
                                                                                       (unsigned)PULSE_INPUT_TASK_STACK_SIZE,
                                                                                       (unsigned)optimalPulseInputTaskStackSize);
                                                                              publishMqttLog("log/stack/pulseInput", logMsg, false);
                                                                            }
                                                                          }
                                                                          if (gTeslaTaskStackHighWater > 0) {
                                                                            uint32_t usedStack = TESLA_TELEMETRY_TASK_STACK_SIZE - gTeslaTaskStackHighWater;
                                                                            uint32_t optimalTeslaTaskStackSize = (usedStack * 5 + 3) / 4; // Multiply by 1.25
                                                                            bool significantDiff = abs((int)TESLA_TELEMETRY_TASK_STACK_SIZE - (int)optimalTeslaTaskStackSize) > 100;
                                                                            if (significantDiff && optimalTeslaTaskStackSize > maxOptimalTeslaTaskStackSize) {
                                                                              maxOptimalTeslaTaskStackSize = optimalTeslaTaskStackSize;
                                                                              char logMsg[128] = {0};
                                                                              snprintf(logMsg,
                                                                                       sizeof(logMsg),
                                                                                       "Change TESLA_TELEMETRY_TASK_STACK_SIZE from: %u to: %u words",
                                                                                       (unsigned)TESLA_TELEMETRY_TASK_STACK_SIZE,
                                                                                       (unsigned)optimalTeslaTaskStackSize);
                                                                              publishMqttLog("log/stack/teslaTelemetry", logMsg, false);
                                                                            }
                                                                          }
                                                                          if (gConfigurationTaskStackHighWater > 0) {
                                                                            uint32_t usedStack = CONFIGURATION_TASK_STACK_SIZE - gConfigurationTaskStackHighWater;
                                                                            uint32_t optimalConfigurationTaskStackSize = (usedStack * 5 + 3) / 4; // Multiply by 1.25
                                                                            bool significantDiff = abs((int)CONFIGURATION_TASK_STACK_SIZE - (int)optimalConfigurationTaskStackSize) > 100;
                                                                            if (significantDiff && optimalConfigurationTaskStackSize > maxOptimalConfigurationTaskStackSize) {
                                                                              maxOptimalConfigurationTaskStackSize = optimalConfigurationTaskStackSize;
                                                                              char logMsg[128] = {0};
                                                                              snprintf(logMsg,
                                                                                       sizeof(logMsg),
                                                                                       "Change CONFIGURATION_TASK_STACK_SIZE from: %u to: %u words",
                                                                                       (unsigned)CONFIGURATION_TASK_STACK_SIZE,
                                                                                       (unsigned)optimalConfigurationTaskStackSize);
                                                                              publishMqttLog("log/stack/configuration", logMsg, false);
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
                                 int &lastDateKey,
                                 uint32_t &nextCheckMs,
                                 bool &bootTelemetryToSend,
                                 bool &pendingTelemetryToSend,
                                 float &pendingEnergyKwh) {
  static uint32_t lastTimeFailLogMs = 0;
  static int lastProcessedDailyTelemetryDateKey = -1;

  if (bootTelemetryToSend && WiFi.status() == WL_CONNECTED) {
    float energyKwh = 0.0f;
    if (getLatestEnergyKwh(&energyKwh)) {
      if (passTeslaTelemetryToGoogleSheets(networkParams, energyKwh)) {
        bootTelemetryToSend = false;
        publishMqttLog(MQTT_LOG_SUFFIX, "Boot telemetry queued", false);
      }
    }
  }

  if (pendingTelemetryToSend && WiFi.status() == WL_CONNECTED) {
    if (passTeslaTelemetryToGoogleSheets(networkParams, pendingEnergyKwh)) {
      pendingTelemetryToSend = false;
      publishMqttLog(MQTT_LOG_SUFFIX, "Pending telemetry queued", false);
    }
  }

  if (millis() >= nextCheckMs) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      int year = timeinfo.tm_year + 1900;
      int currentDateKey = (year * 1000) + timeinfo.tm_yday;
      bool dayChanged = (lastDateKey != -1 && currentDateKey != lastDateKey);

      if (dayChanged && currentDateKey > lastProcessedDailyTelemetryDateKey) {
        float energyKwh = 0.0f;
        if (getLatestEnergyKwh(&energyKwh)) {
          if (WiFi.status() == WL_CONNECTED) {
            if (passTeslaTelemetryToGoogleSheets(networkParams, energyKwh)) {
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
        lastProcessedDailyTelemetryDateKey = currentDateKey;
      }
      lastDateKey = currentDateKey;

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

                                                    #ifdef BOOT_DIAGNOSTICS_LOGGING
                                                    static const char* resetReasonToString(esp_reset_reason_t reason) {
                                                      switch (reason) {
                                                        case ESP_RST_UNKNOWN:   return "UNKNOWN";
                                                        case ESP_RST_POWERON:   return "POWERON";
                                                        case ESP_RST_EXT:       return "EXT";
                                                        case ESP_RST_SW:        return "SW";
                                                        case ESP_RST_PANIC:     return "PANIC";
                                                        case ESP_RST_INT_WDT:   return "INT_WDT";
                                                        case ESP_RST_TASK_WDT:  return "TASK_WDT";
                                                        case ESP_RST_WDT:       return "WDT";
                                                        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
                                                        case ESP_RST_BROWNOUT:  return "BROWNOUT";
                                                        case ESP_RST_SDIO:      return "SDIO";
                                                        default:                return "OTHER";
                                                      }
                                                    }

                                                    static void publishBootDiagnosticsOnce() {
                                                      static bool bootDiagnosticsPublished = false;
                                                      if (bootDiagnosticsPublished) {
                                                        return;
                                                      }

                                                      if (WiFi.status() != WL_CONNECTED) {
                                                        return;
                                                      }

                                                      esp_reset_reason_t reason = esp_reset_reason();
                                                      unsigned long uptimeSeconds = millis() / 1000UL;

                                                      char logMsg[180] = {0};
                                                      snprintf(logMsg,
                                                              sizeof(logMsg),
                                                              "Boot diagnostics: reset_reason=%s(%d), boot_count=%lu, uptime_s=%lu",
                                                              resetReasonToString(reason),
                                                              (int)reason,
                                                              (unsigned long)sBootCount,
                                                              uptimeSeconds);

                                                      char resetReasonPayload[96] = {0};
                                                      snprintf(resetReasonPayload,
                                                              sizeof(resetReasonPayload),
                                                              "%s(%d),boot_count=%lu,uptime_s=%lu",
                                                              resetReasonToString(reason),
                                                              (int)reason,
                                                              (unsigned long)sBootCount,
                                                              uptimeSeconds);

                                                      char bootTimePayload[64] = {0};
                                                      struct tm timeinfo;
                                                      if (getLocalTime(&timeinfo)) {
                                                        snprintf(bootTimePayload,
                                                                sizeof(bootTimePayload),
                                                                "%04d-%02d-%02d %02d:%02d:%02d",
                                                                timeinfo.tm_year + 1900,
                                                                timeinfo.tm_mon + 1,
                                                                timeinfo.tm_mday,
                                                                timeinfo.tm_hour,
                                                                timeinfo.tm_min,
                                                                timeinfo.tm_sec);
                                                      } else {
                                                        snprintf(bootTimePayload,
                                                                sizeof(bootTimePayload),
                                                                "time-unavailable uptime_s=%lu",
                                                                uptimeSeconds);
                                                      }

                                                      bool statusLogged = publishMqttLogStatus(logMsg, false);
                                                      bool resetReasonPublished = publishMqttResetReason(resetReasonPayload, true);
                                                      bool bootTimePublished = publishMqttBootTime(bootTimePayload, true);

                                                      if (statusLogged && resetReasonPublished && bootTimePublished) {
                                                        bootDiagnosticsPublished = true;
                                                      }
                                                    }
                                                    #endif

                                                    #ifdef VERIFY_LOCAL_TIME
                                                    /* 
                                                    * TOBE REMOVED: Checks if local time is valid and logs the current time and epoch to MQTT for debugging.
                                                    * This is useful for verifying that NTP time synchronization is working correctly and that the device 
                                                    * has a valid local time, which is important for the daily telemetry logic and any time-based functionality.
                                                    * This function will log the local time and epoch time to MQTT every 60 seconds when WiFi is connected, 
                                                    * and also immediately after WiFi reconnects. It checks if the year is plausible (>= 2024) to help identify 
                                                    * if the time is not set correctly (e.g., if it defaults to 1970).
                                                    */

                                                    static void verifyLocalTimeHealth() {
                                                      static bool hadConnectedWifi = false;
                                                      static uint32_t nextLogMs = 0;

                                                      bool wifiConnected = (WiFi.status() == WL_CONNECTED);
                                                      if (!wifiConnected) {
                                                        hadConnectedWifi = false;
                                                        return;
                                                      }

                                                      uint32_t nowMs = millis();
                                                      bool logBecauseWifiReconnected = !hadConnectedWifi;
                                                      bool logBecausePeriodicCheck = nowMs >= nextLogMs;
                                                      if (!logBecauseWifiReconnected && !logBecausePeriodicCheck) {
                                                        return;
                                                      }

                                                      hadConnectedWifi = true;
                                                      nextLogMs = nowMs + 60000U;

                                                      struct tm timeinfo;
                                                      if (!getLocalTime(&timeinfo, 1000)) {
                                                        publishMqttLogStatus("Time verify: getLocalTime failed", false);
                                                        return;
                                                      }

                                                      time_t epochNow = 0;
                                                      time(&epochNow);

                                                      char tzLabel[16] = {0};
                                                      strftime(tzLabel, sizeof(tzLabel), "%Z", &timeinfo);

                                                      int year = timeinfo.tm_year + 1900;
                                                      bool plausibleYear = year >= 2024;

                                                      char logMsg[160] = {0};
                                                      snprintf(logMsg,
                                                              sizeof(logMsg),
                                                              "Time verify: %04d-%02d-%02d %02d:%02d:%02d %s, epoch=%ld, plausible=%s",
                                                              year,
                                                              timeinfo.tm_mon + 1,
                                                              timeinfo.tm_mday,
                                                              timeinfo.tm_hour,
                                                              timeinfo.tm_min,
                                                              timeinfo.tm_sec,
                                                              tzLabel[0] ? tzLabel : "TZ?",
                                                              (long)epochNow,
                                                              plausibleYear ? "yes" : "no");

                                                      publishMqttLogStatus(logMsg, false);
                                                    }
                                                    #endif
