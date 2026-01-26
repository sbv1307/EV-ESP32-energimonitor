#define DEBUG

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>

#include <wait_for_any_key.h>
#include "config/config.h"
#include "config/privateConfig.h"
#include "globals/globals.h"
#include "network/NetworkTask.h"
#include "pulsInput/PulseInputTask.h"

/*
 * ######################################################################################################################################
 * ######################################################################################################################################
 *                       V  A  R  I  A  B  L  E      D  E  F  I  N  A  I  T  O  N  S
 * ######################################################################################################################################
 * ######################################################################################################################################
 */
static String versionString = String(SKETCH_VERSION) + ". Build at: " + BUILD_TIMESTAMP;
static TaskParams_t networkParams = { .sketchVersion = versionString.c_str(), .nvsNamespace = NVS_NAMESPACE };

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
src/
 │── main.cpp          // setup() and loop()
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
//  #ifdef DEBUG
  wait_for_any_key( SKETCH_VERSION + String(". Build at: ") + String(BUILD_TIMESTAMP));
//  #endif
  // Include ESP32_NW_Setup library will setup and store WiFi credentials if not present
  // for now, we will hardcode WiFi credentials for testing

  Preferences prefs;
  prefs.begin( NVS_NAMESPACE, false);
  if ( !prefs.isKey("ssid") || prefs.getString("ssid", "").isEmpty()) {
                                                                      #ifdef DEBUG
                                                                        Serial.println("Main: NVS NaMESPACE not found. Creating with default values.");
                                                                      #endif

    prefs.putString("ssid", "DIGIFIBRA-D3s4");
    prefs.putString("pass", "FHkYs5k6k9Tc");
    prefs.putString("mqttIP", "192.168.1.136");
    prefs.putString("mqttPort", "1883");
    prefs.putString("mqttUser", "");
    prefs.putString("mqttPass", "");  
  } else {

                                                                      #ifdef DEBUG
                                                                        Serial.println("Main: NVS NAMESPACE found. Using stored values.");
                                                                      #endif

  }
  prefs.end();

  initializeGlobals( &networkParams );

  startNetworkTask( &networkParams );

  //vTaskDelay(pdMS_TO_TICKS(5000));
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
}
