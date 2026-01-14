#define DEBUG

#include "MqttClient.h"
#include "../config/privateConfig.h"

#include <Preferences.h>
#include <PubSubClient.h>
#include <WiFi.h>

#define RETAINED true                   // Used in MQTT publications. Can be changed during development and bugfixing.

static WiFiClient wifiClient;
static PubSubClient mqttClient(wifiClient);

static QueueHandle_t mqttQueue = nullptr;
static String mqttBrokerIP;
static uint16_t mqttBrokerPort;


static void reconnect() {
  while (!mqttClient.connected()) {
    String will = String ( MQTT_clientID + MQTT_ONLINE);

                                                             #ifdef DEBUG
                                                             Serial.print("Attempting MQTT connection...");
                                                             #endif
//TO_BE_REMOVED   if (mqttClient.connect("esp32-client")) {
    if (mqttClient.connect(MQTT_clientID.c_str(), PRIVATE_Default_MQTT_USER.c_str(), 
                               PRIVATE_Default_MQTT_PASS.c_str(), will.c_str(), 1, RETAINED, "False")) {
                                                             #ifdef DEBUG
                                                             Serial.println("connected");
                                                             #endif
                                                             
      // Once connected, publish an announcement...
      mqttEnqueuePublish(will.c_str(), "True", RETAINED);
      

      String totalSetTopic = String(MQTT_clientID + "/+" + MQTT_SUFFIX_TOTAL);
      mqttClient.subscribe(totalSetTopic.c_str(), 1);

                                                              #ifdef DEBUG
                                                              Serial.println("MQTT connected");
                                                              #endif

    } else {

                                                              #ifdef DEBUG
                                                              Serial.print("MQTT failed, rc=");
                                                              Serial.print(mqttClient.state());
                                                              Serial.println(" try again in 2 seconds");
                                                              #endif

      vTaskDelay(pdMS_TO_TICKS(2000));
    }
  }
}

void mqttInit(TaskParams_t* params) {
  Preferences pref;
  pref.begin( params->nvsNamespace, false);
  mqttBrokerIP = pref.getString(MQTTIPnameSpaceKey, PRIVATE_Default_MQTT_SERVER);
  mqttBrokerPort = pref.getString(MQTTPortnameSpaceKey, String(PRIVATE_Default_MQTT_PORT).c_str()).toInt();
  pref.end();

                                                          #ifdef DEBUG
                                                          Serial.println("MQTT broker IP: " + mqttBrokerIP + ", port: " + String(mqttBrokerPort) );
                                                          #endif

  mqttClient.setServer(mqttBrokerIP.c_str(), mqttBrokerPort);
  
  mqttQueue = xQueueCreate(10, sizeof(MqttMessage));
  if (!mqttQueue) {
    Serial.println("MQTT queue creation failed!: MQTT broker IP: " + mqttBrokerIP + ", port: " + String(mqttBrokerPort) );
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

void mqttLoop() {
  if (!mqttClient.connected()) {
    reconnect();
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
