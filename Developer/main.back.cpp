#include <Arduino.h>

// Include necessary BLE (Bluetooth Low Energy) libraries
#include <NimBLEDevice.h>
#include <Preferences.h>

#define SKETCH_VERSION "Esp32 MQTT interface for EV ESP32 Energy monitoring - Version 1.0.0"
#define DEBUG

// BLE definitions UUIDs (can be anything, but must match client)
#define SERVICE_UUID       "12345678-1234-1234-1234-1234567890ab"
#define SSID_CHAR_UUID     "12345678-1234-1234-1234-1234567890ac"
#define PASS_CHAR_UUID     "12345678-1234-1234-1234-1234567890ad"



char p_buffer[150];
#define P(str) (strcpy_P(p_buffer, PSTR(str)), p_buffer)

/*
 * ##################################################################################################
 * ##################################################################################################
 * ##################################################################################################
 *                       V  A  R  I  A  B  L  E      D  E  F  I  N  A  I  T  O  N  S
 * ##################################################################################################
 * ##################################################################################################
 * ##################################################################################################
 */


 /*
 * ##################################################################################################
 * ##################################################################################################
 * ##################################################################################################
 * ##########################   f u n c t i o n    d e c l a r a t i o n s  #########################
 * ##################################################################################################
 * ##################################################################################################
 * ##################################################################################################
 */

// Callback for connection events

/*
* ###################################################################################################
* ###################################################################################################
* ###################################################################################################
* ###################   C A L L B A C K S     D E F I N I T I O N S  ################################
* ###################################################################################################
* ###################################################################################################
* ###################################################################################################
*      Best recommendation for Arduino / NimBLE code:  Keep the class definition above setup()
*/

// Creates an instance of the Preferences class, which is ESP32’s non-volatile storage (NVS) interface.
Preferences prefs;

/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Callback for SSID <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */
class SSIDCallback : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pChar) {
    std::string value = pChar->getValue();



//                                                              #ifdef DEBUG
    Serial.print("SSID received: ");
    Serial.println(value.c_str());
//                                                              #endif 

/*
    prefs.begin("wifi", false);
    prefs.putString("ssid", value.c_str());
    prefs.end();
*/
  }
};

/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Callback for Password <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< */
class PassCallback : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pChar)  {
    std::string value = pChar->getValue();
                                                              #ifdef DEBUG
                                                                Serial.print("Password received: ");
                                                                Serial.println(value.c_str());
                                                              #endif
/*
    prefs.begin("wifi", false);
    prefs.putString("pass", value.c_str());
    prefs.end();
*/
  }
};

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
  /*
  * To prevent initialaction failures caused by power interruption during physicalmounting, 
  * make a delay to wait for a steady power supply.
  */
  delay( 2000);

 
  
                                                              #ifdef DEBUG
                                                                Serial.begin(115200);
    while (!Serial) {
    ;  // Wait for serial connections before proceeding
    }
    Serial.println("Hit [Enter] to start!");
    while (!Serial.available()) {
    ;  // In order to prevent unattended execution, wait for [Enter].
    }
    while (Serial.available()) {
    char c = Serial.read();  // Empty input buffer.
    }
                                                                
                                                              #endif
  
  // String ssid = prefs.getString("ssid", "");

  // Begin BLE Setup >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

  // Init BLE
  NimBLEDevice::init("ESP32-Setup");
  NimBLEServer *server = NimBLEDevice::createServer();

  // Create service
  NimBLEService *service = server->createService(SERVICE_UUID);

  // SSID characteristic
  NimBLECharacteristic *ssidChar =
      service->createCharacteristic(
        SSID_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE
      );
  ssidChar->setCallbacks(new SSIDCallback());

  // Password characteristic
  NimBLECharacteristic *passChar =
      service->createCharacteristic(
        PASS_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE
      );
  passChar->setCallbacks(new PassCallback());

  service->start();

  // Advertising
  /*
  NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponseData(NimBLEAdvertisementData()); // replaces setScanResponse(true)
  advertising->start();
  */
  
  NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();

  NimBLEAdvertisementData advData;
  advData.setName("ESP32-Setup");        // 👈 THIS is the key line
  advData.setFlags(0x06);           // BLE discoverable + BR/EDR not supported
  advertising->setAdvertisementData(advData);

  advertising->addServiceUUID(SERVICE_UUID);
  advertising->start();



                                                              #ifdef DEBUG
                                                                Serial.println("BLE ready. Waiting for client...");
                                                              #endif


// END BLE Setup <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<



}

/*
 * ###################################################################################################
 * ###################################################################################################
 * ###################################################################################################
 * ###################   L O O P     B e g i n  ######################################################
 * ###################################################################################################
 * ###################################################################################################
 * ###################################################################################################
 */
void loop() {


}

/*
 * ###################################################################################################
 * ###################################################################################################
 * ###################################################################################################
 * ###################   F  U  N  C  T  I  O  N      D  E  F  I  N  I  T  I  O  N  S #################
 * ###################################################################################################
 * ###################################################################################################
 * ###################################################################################################
*/
// put function definitions here:
int myFunction(int x, int y) {
  return x + y;
}




/*
 * ######################################################################################################################################
 * ######################################################################################################################################
 *                       E N D      O F      F I L E
 * ######################################################################################################################################
 * ######################################################################################################################################
 */
