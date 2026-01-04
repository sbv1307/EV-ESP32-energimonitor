/*
From https://chatgpt.com/c/694d50f1-3b68-8331-993b-19e5a2aa393b 
Below is a complete, production-ready BLE Wi-Fi provisioning example using the same BLE stack that you already confirmed works.

✅ Uses BLEDevice (Bluedroid)
✅ Works with nRF Connect
✅ Receives SSID + Password
✅ Saves to Preferences (NVS)
✅ Attempts Wi-Fi connection
✅ Reports status via BLE notify
✅ No NimBLE, no instability

ESP32 BLE Wi-Fi Provisioning (Working Version)
*/


#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <WiFi.h>
#include <Preferences.h>

// ===== UUIDs =====
#define SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define SSID_CHAR_UUID     "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define PASS_CHAR_UUID     "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define STATUS_CHAR_UUID   "6E400004-B5A3-F393-E0A9-E50E24DCCA9E"

// ===== Globals =====
// Preferences prefs;
BLECharacteristic *statusChar;
String wifiSSID;
String wifiPASS;
bool deviceConnected = false;

// ===== BLE Callbacks =====
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*) {
    deviceConnected = true;
    Serial.println("BLE connected");
  }

  void onDisconnect(BLEServer*) {
    deviceConnected = false;
    Serial.println("BLE disconnected");
  }
};

// ===== SSID Callback =====
class SSIDCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) {
    wifiSSID = pChar->getValue().c_str();
    Serial.println("SSID received: " + wifiSSID);
  }
};

// ===== Password Callback =====
class PassCallback : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) {
    wifiPASS = pChar->getValue().c_str();
    Serial.println("Password received");

    // Save credentials
    // .putString("ssid", wifiSSID);
    // prefs.putString("pass", wifiPASS);

    connectToWiFi();
  }
};

// ===== WiFi Connect =====
void connectToWiFi() {
  Serial.println("Connecting to WiFi...");
  statusChar->setValue("Connecting...");
  statusChar->notify();

  WiFi.begin(wifiSSID.c_str(), wifiPASS.c_str());

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries++ < 20) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    statusChar->setValue("Connected");
  } else {
    Serial.println("\nWiFi failed");
    statusChar->setValue("Failed");
  }

  statusChar->notify();
}

// ===== Setup =====
void setup() {
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

//  prefs.begin("wifi", false);

  BLEDevice::init("ESP32-Setup");

  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService *service = server->createService(SERVICE_UUID);

  // SSID
  BLECharacteristic *ssidChar =
    service->createCharacteristic(
      SSID_CHAR_UUID,
      BLECharacteristic::PROPERTY_WRITE
    );
  ssidChar->setCallbacks(new SSIDCallback());

  // Password
  BLECharacteristic *passChar =
    service->createCharacteristic(
      PASS_CHAR_UUID,
      BLECharacteristic::PROPERTY_WRITE
    );
  passChar->setCallbacks(new PassCallback());

  // Status
  statusChar =
    service->createCharacteristic(
      STATUS_CHAR_UUID,
      BLECharacteristic::PROPERTY_NOTIFY
    );
  statusChar->addDescriptor(new BLE2902());

  service->start();

  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->start();

  Serial.println("BLE Wi-Fi Provisioning Ready");
}

// ===== Loop =====
void loop() {
  delay(1000);
}
