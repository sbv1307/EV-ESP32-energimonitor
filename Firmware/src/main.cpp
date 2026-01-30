#define NONE_HEADLESS // Define to disable headless wait_for_any_key functionality and use serial output instead
#define DEBUG         // Enable debug serial output. Requires NONE_HEADLESS to be defined.
#define STACK_WATERMARK // Enable stack watermarking debug output. Requires NONE_HEADLESS to be defined.

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>

#include <wait_for_any_key.h>
#include "config.h"
#include "privateConfig.h"
#include "globals.h"
#include "NetworkTask.h"
#include "PulseInputTask.h"

/*
 * ######################################################################################################################################
 * ######################################################################################################################################
 *                       V  A  R  I  A  B  L  E      D  E  F  I  N  A  I  T  O  N  S
 * ######################################################################################################################################
 * ######################################################################################################################################
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
                                                              Serial.printf("Starting FreeRTOS: Memory Usage\nInitial Free Heap: %u bytes\n", xPortGetFreeHeapSize());
                                                              #endif
                                                              

/*
Preferences prefs;
prefs.begin( NVS_NAMESPACE, false);
if ( prefs.isKey("ssid") || prefs.getString("ssid", "").isEmpty()) {
  #ifdef DEBUG
  Serial.println("Main: NVS NaMESPACE not found. Creating with default values.");
  #endif
  
  prefs.putString("ssid", "SUIT FRENTE AL MAR 2.4G");
  prefs.putString("pass", "Frentealmar");
  prefs.putString("mqttIP", "192.168.100.245");
  prefs.putString("mqttPort", "1883");
  prefs.putString("mqttUser", "");
  prefs.putString("mqttPass", "");  
} else {
  
#ifdef DEBUG
Serial.println("Main: NVS NAMESPACE found. Using stored values.");
#endif

}
prefs.end();
*/

  initializeGlobals( &networkParams );

  // Debug: Print initialized parameters
  Serial.println("\nMain: WiFi SSID: " + String(networkParams.wifiSSID) + "\n");

  startNetworkTask( &networkParams );

  vTaskDelay(pdMS_TO_TICKS(100000));
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
  

  vTaskDelay(pdMS_TO_TICKS(5000));
  PulseInputISR();

                                        #ifdef STACK_WATERMARK
                                          unsigned long stackNow = millis();
                                          if (stackNow - lastStackLog >= 5000) {
                                            lastStackLog = stackNow;
                                            if (gNetworkTaskStackHighWater > 0) {
                                              Serial.printf("NetworkTask stack free: %u words (%u bytes)\n",
                                                (unsigned)gNetworkTaskStackHighWater,
                                                (unsigned)(gNetworkTaskStackHighWater * sizeof(StackType_t)));
                                            }
                                            if (gWifiConnTaskStackHighWater > 0) {
                                              Serial.printf("WifiConnectionTask stack free: %u words (%u bytes)\n",
                                                (unsigned)gWifiConnTaskStackHighWater,
                                                (unsigned)(gWifiConnTaskStackHighWater * sizeof(StackType_t)));
                                            }
                                            if (gPulseInputTaskStackHighWater > 0) {
                                              Serial.printf("PulseInputTask stack free: %u words (%u bytes)\n",
                                                (unsigned)gPulseInputTaskStackHighWater,
                                                (unsigned)(gPulseInputTaskStackHighWater * sizeof(StackType_t)));
                                            }
                                            Serial.printf("Free Heap: %u bytes\n", xPortGetFreeHeapSize());
                                          }
                                        #endif

}
