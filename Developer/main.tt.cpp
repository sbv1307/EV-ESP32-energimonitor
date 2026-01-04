
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// UUIDs (can be anything, but must match client)
#define SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_RX   "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" // Write
#define CHARACTERISTIC_TX   "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" // Notify

BLECharacteristic *txCharacteristic;
BLECharacteristic *rxCharacteristic;

bool deviceConnected = false;

// Callback for connection events
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("Device connected");
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("Device disconnected");
  }
};

// Callback when data is written from phone
class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();

    Serial.print("Received: ");
    Serial.println(value.c_str());
  }
};


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
  
  BLEDevice::init("ESP32_BLE");

  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  // TX characteristic (notify)
  txCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_TX,
                        BLECharacteristic::PROPERTY_NOTIFY
                      );
  txCharacteristic->addDescriptor(new BLE2902());

  // RX characteristic (write)
  rxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_RX,
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_WRITE_NR
  );
  rxCharacteristic->setCallbacks(new MyCallbacks());


  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();

  Serial.println("BLE ready. Waiting for connection...");
}

void loop() {
  if (deviceConnected) {
    static int counter = 0;
    String msg = "Hello BLE #" + String(counter++);

    txCharacteristic->setValue(msg.c_str());
    txCharacteristic->notify();

    Serial.println("Sent: " + msg);
    delay(1000);
  }
}
