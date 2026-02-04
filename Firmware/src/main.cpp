#define NONE_HEADLESS // Define to disable headless wait_for_any_key functionality and use serial output instead
#define DEBUG         // Enable debug serial output. Requires NONE_HEADLESS to be defined.
//#define STACK_WATERMARK // Enable stack watermarking debug output. Requires NONE_HEADLESS to be defined.

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>

#include <wait_for_any_key.h>
#include "config.h"
#include "privateConfig.h"
#include "globals.h"
#include "NetworkTask.h"
#include "PulseInputTask.h"
#include "TeslaSheets.h"

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

 Functions are now declared in their respective header files and only included here.
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

  startNetworkTask( &networkParams );
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
  static int NetworkTaskMaxStack = 0;
  static int WifiConnTaskMaxStack = 0;
  static int PulseInputTaskMaxStack = 0;
  static int loopCounter = 0;
  static bool testTeslaCallDone = false;
  
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

  startPulseInputTask( &networkParams );  // Ensure Pulse Count Task is running. If already running, this does nothing.

  if (WiFi.status() == WL_CONNECTED) {
    loopCounter++;
    if (!testTeslaCallDone && loopCounter >= 3) {
      
                                                    #ifdef DEBUG
                                                    Serial.println("Main: Testing Tesla API call to send telemetry to Google Sheets...");
                                                    #endif

      sendTeslaTelemetryToGoogleSheets(&networkParams, 0.0f);
      testTeslaCallDone = true;

                                            #ifdef DEBUG
                                            Serial.println("Main: Tesla API test call completed."); 
                                            #endif

    }
  }

  vTaskDelay(pdMS_TO_TICKS(5000));
  PulseInputISR();

                                                                      #ifdef STACK_WATERMARK
                                                                        unsigned long stackNow = millis();
                                                                        if (stackNow - lastStackLog >= 5000) {
                                                                          Serial.println(">>>>>>>>>>>>>>>>>>>>>>   Checking Stack sizes   <<<<<<<<<<<<<<<<<<<<<<<<<<<<");
                                                                          lastStackLog = stackNow;
                                                                          if (gNetworkTaskStackHighWater > 0) {
                                                                            uint32_t usedStack = NETWORK_TASK_STACK_SIZE - gNetworkTaskStackHighWater;
                                                                            uint32_t optimalNetworkTaskStackSize = (usedStack * 5 + 3) / 4; // Multiply by 1.25
                                                                            if ( abs( (int)NETWORK_TASK_STACK_SIZE - (int)optimalNetworkTaskStackSize) > 100 )  // Only log if there's a significant difference
                                                                              Serial.printf("Change networkTaskStackSize to: %u words (%u bytes)\n",
                                                                                (unsigned)optimalNetworkTaskStackSize,
                                                                                (unsigned)(optimalNetworkTaskStackSize * sizeof(StackType_t))
                                                                              );
                                                                          }
                                                                          if (gWifiConnTaskStackHighWater > 0) {
                                                                            uint32_t usedStack = WIFI_CONNECTION_TASK_STACK_SIZE - gWifiConnTaskStackHighWater;
                                                                            uint32_t optimalWifiConnTaskStackSize = (usedStack * 5 + 3) / 4; // Multiply by 1.25
                                                                            if ( abs( (int)WIFI_CONNECTION_TASK_STACK_SIZE - (int)optimalWifiConnTaskStackSize) > 100 )  // Only log if there's a significant difference
                                                                              Serial.printf("Change wifiConnectionTaskStackSize to: %u words (%u bytes)\n",
                                                                                (unsigned)optimalWifiConnTaskStackSize,
                                                                                (unsigned)(optimalWifiConnTaskStackSize * sizeof(StackType_t))
                                                                              );
                                                                          }
                                                                          if (gPulseInputTaskStackHighWater > 0) {
                                                                            uint32_t usedStack = PULSE_INPUT_TASK_STACK_SIZE - gPulseInputTaskStackHighWater;
                                                                            uint32_t optimalPulseInputTaskStackSize = (usedStack * 5 + 3) / 4; // Multiply by 1.25
                                                                            if ( abs( (int)PULSE_INPUT_TASK_STACK_SIZE - (int)optimalPulseInputTaskStackSize) > 100 )  // Only log if there's a significant difference
                                                                              Serial.printf("Change pulseInputTaskStackSize to: %u words (%u bytes)\n",
                                                                                (unsigned)optimalPulseInputTaskStackSize,
                                                                                (unsigned)(optimalPulseInputTaskStackSize * sizeof(StackType_t))
                                                                              );
                                                                          }
                                                                          Serial.printf("Initial Free Heap: %u bytes\n", (unsigned)gInitialFreeHeapSize);
                                                                          Serial.printf("Current Free Heap: %u bytes\n", xPortGetFreeHeapSize());
                                                                          Serial.println("=============================================================================");
                                                                        }
                                                                      #endif

}
