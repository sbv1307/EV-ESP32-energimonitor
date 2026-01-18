//#define DEBUG
#include <ArduinoJson.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include "MqttMessage.h"
#include "MqttClient.h"
#include "../config/config.h"
#include "../config/privateConfig.h"


#define RETAINED true                   // Used in MQTT publications. Can be changed during development and bugfixing.

static String mqttDeviceNameWithMac;
static String mqttClientWithMac;

static WiFiClient wifiClient;
static PubSubClient mqttClient(wifiClient);

static QueueHandle_t mqttQueue = nullptr;
static String mqttBrokerIP;
static uint16_t mqttBrokerPort;


/*
  * ###################################################################################################
  *                  R E C O N N E C T
  * A clean, idiomatic ESP32 + FreeRTOS module layout that keeps OTA + MQTT in one network task, 
  * while staying Arduino-friendly.
  * ###################################################################################################
  *  
  */  

static void reconnect(TaskParams_t* params) {
  // Try to connect only once per call to avoid blocking
  if (!mqttClient.connected()) {
    // Throttle connection attempts to prevent watchdog issues
    static unsigned long lastAttempt = 0;
    unsigned long now = millis();
    if (now - lastAttempt < 5000) {  // Wait at least 5 seconds between attempts
      return;
    }
    lastAttempt = now;
    
    String will = String ( MQTT_PREFIX + mqttDeviceNameWithMac + MQTT_ONLINE);

                                                             #ifdef DEBUG
                                                             Serial.print("Attempting MQTT connection...");
                                                             #endif

    // Yield to watchdog before blocking connect call
    vTaskDelay(pdMS_TO_TICKS(10));
    
    if (mqttClient.connect( mqttClientWithMac.c_str(),
                            params->mqttUsername, 
                            params->mqttPassword,
                            will.c_str(),
                            1,
                            RETAINED, "False")) 
    {

      // Once connected, publish will message and 
      mqttEnqueuePublish(will.c_str(), "True", RETAINED);

      publish_sketch_version( params);
      /*************************************************************************************
       *************************************************************************************
       *************************************************************************************
       * Subscriptions to topics for receiving set commands are done here
       *************************************************************************************
       *************************************************************************************
       *************************************************************************************/
      
      String topicString = String(MQTT_PREFIX + "/+" + MQTT_SUFFIX_TOTAL);
      mqttClient.subscribe(topicString.c_str(), 1);

                                                              #ifdef DEBUG
                                                              Serial.println("MQTT connected");
                                                              #endif

    } else {

                                                              #ifdef DEBUG
                                                              Serial.print("MQTT failed, rc=");
                                                              Serial.print(mqttClient.state());
                                                              Serial.println(" retrying...");
                                                              #endif
    }
  }
}

void mqttInit(TaskParams_t* params) {
  
  initializeMQTTGlobals();

                                                          #ifdef DEBUG
                                                          Serial.println("MQTT broker IP: " + String(params->mqttBrokerIP) + ", port: " + String(params->mqttBrokerPort) );
                                                          #endif

  mqttClient.setServer(params->mqttBrokerIP, params->mqttBrokerPort);
  mqttClient.setSocketTimeout(3);  // Set 3 second timeout to prevent watchdog triggers
  mqttClient.setCallback(mqttCallback);


  mqttQueue = xQueueCreate(10, sizeof(MqttMessage));
  if (!mqttQueue) {
    Serial.println("MQTT queue creation failed!: MQTT broker IP: " + String(params->mqttBrokerIP) + ", port: " + String(params->mqttBrokerPort) );
  }
}

bool mqttEnqueuePublish(const char* topic, const char* payload, bool retain) {
  if (!mqttQueue) return false;

  MqttMessage msg{};
  strncpy(msg.topic, topic, MQTT_TOPIC_LEN - 1);
  strncpy(msg.payload, payload, MQTT_PAYLOAD_LEN - 1);
  msg.retain = retain;

  return xQueueSend(mqttQueue, &msg, 0) == pdTRUE;
}

void mqttLoop(TaskParams_t* params) {
  if (!mqttClient.connected()) {
    reconnect(params);
  }

  mqttClient.loop();

  // Process outgoing messages
  MqttMessage msg;
  while (xQueueReceive(mqttQueue, &msg, 0) == pdTRUE) {

                                          #ifdef DEBUG
                                          Serial.println("Publishing to topic: " + String(msg.topic) + " payload: " + String(msg.payload) + " retain: " + String(msg.retain) );
                                          #endif

    mqttClient.publish(msg.topic, msg.payload, msg.retain);
  }
}

bool mqttIsConnected() {
  return mqttClient.connected();
}

/* ###################################################################################################
 *                  P U B L I S H   S K E T C H   V E R S I O N
 * ###################################################################################################
 *  As the sketch will execute in silence, one way to se which version of the SW is running will be
 *  to subscribe to topic defined as (MQTT_PREFIX + mqttDeviceNameWithMac + MQTT_SKETCH_VERSION)
 *  on the MQTT broker, configured in the privateConfig.h file.
 *  From command: 
 *  mosquitto_sub -h 192.168.11.34 -p 1883 -t 'ev-e-monitor/+/sketch_version' -u myuser -P mypassword -v
 *
 *  This will return the value of (SKETCH_VERSION)
 */
void publish_sketch_version(TaskParams_t* params)   // Publish only once at every reboot.
{
  IPAddress ip = WiFi.localIP();
  String versionTopic = String(MQTT_PREFIX + mqttDeviceNameWithMac + MQTT_SKETCH_VERSION);
  String versionMessage = String(params->sketchVersion + String("\nConnected to SSID: \'") +\
                                  String(params->wifiSSID) + String("\' at: ") +\
                                  String(ip[0]) + String(".") +\
                                  String(ip[1]) + String(".") +\
                                  String(ip[2]) + String(".") +\
                                  String(ip[3]));

  mqttEnqueuePublish(versionTopic.c_str(), versionMessage.c_str(), RETAINED);
}

void initializeMQTTGlobals()
{
  // >>>>>>>>>>>>>   Set globals for MQTT Device and Client   <<<<<<<<<<<<<<<<<<
  uint8_t mac[6];
  WiFi.macAddress(mac);

  char macStr[13] = {0};
  sprintf(macStr, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  mqttDeviceNameWithMac = String(MQTT_DEVICE_NAME + macStr);
  mqttClientWithMac = String(MQTT_CLIENT + macStr);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  JsonDocument doc;                         // 
  byte IRQ_PIN_reference = 0;
  String topicString = String(topic);


}