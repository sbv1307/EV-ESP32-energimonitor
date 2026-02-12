//#define DEBUG
#include <ArduinoJson.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <time.h>

#include "MqttMessage.h"
#include "MqttClient.h"
#include "config.h"
#include "privateConfig.h"
#include "PulseInputTask.h"


#define RETAINED true       // Used in MQTT publications. Can be changed during development and bugfixing.

static String mqttDeviceNameWithMac;
static String mqttClientWithMac;

static WiFiClient wifiClient;
static PubSubClient mqttClient(wifiClient);

static QueueHandle_t mqttQueue = nullptr;
static QueueHandle_t mqttRxQueue = nullptr;
static volatile bool mqttPaused = false;
static TaskParams_t* mqttParams = nullptr;

struct MqttRxMessage {
  char topic[MQTT_TOPIC_LEN];
  char payload[MQTT_PAYLOAD_LEN];
  uint16_t length;
};

static void formatLogTimestamp(char* buffer, size_t bufferSize) {
  if (!buffer || bufferSize == 0) {
    return;
  }

  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    snprintf(buffer,
             bufferSize,
             "%04d-%02d-%02d %02d:%02d:%02d",
             timeinfo.tm_year + 1900,
             timeinfo.tm_mon + 1,
             timeinfo.tm_mday,
             timeinfo.tm_hour,
             timeinfo.tm_min,
             timeinfo.tm_sec);
    return;
  }

  snprintf(buffer, bufferSize, "ms:%lu", (unsigned long)millis());
}


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
                                                             Serial.print("MqttClient:  rc= " + String(mqttClient.state()) + " Attempting MQTT connection...");
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

      publishMqttConfigurations();

      //publishMqttEnergy(0.0f, 0, 0);  // Initial publish with zero values

      /*************************************************************************************
       *************************************************************************************
       *************************************************************************************
       * Subscriptions to topics for receiving set commands are done here
       *************************************************************************************
       *************************************************************************************
       *************************************************************************************/

      String topicString = String(MQTT_PREFIX + mqttDeviceNameWithMac + MQTT_SUFFIX_SET);
      mqttClient.subscribe(topicString.c_str(), 1);

                                                              #ifdef DEBUG
                                                              Serial.println("MqttClient: MQTT connected");
                                                              #endif

    } else {

                                                              #ifdef DEBUG
                                                              Serial.print("MqttClient: MQTT failed, rc=");
                                                              Serial.print(mqttClient.state());
                                                              Serial.println(" retrying...");
                                                              #endif
    }
  }
}

/* ###################################################################################################
 *                  M Q T T   I N I T
 * ###################################################################################################
 */
void mqttInit(TaskParams_t* params) {
  mqttParams = params;
  
  initializeMQTTGlobals();

                                                          #ifdef DEBUG
                                                          Serial.println("MqttClient: MQTT broker IP: " + String(params->mqttBrokerIP) + ", port: " + String(params->mqttBrokerPort) );
                                                          #endif

  mqttClient.setServer(params->mqttBrokerIP, params->mqttBrokerPort); 
  mqttClient.setSocketTimeout(3);  // Set 3 second timeout to prevent watchdog triggers
  mqttClient.setCallback(mqttCallback);


  mqttQueue = xQueueCreate(10, sizeof(MqttMessage));
  if (!mqttQueue) {
    Serial.println("MqttClient: MQTT queue creation failed!: MQTT broker IP: " + String(params->mqttBrokerIP) + ", port: " + String(params->mqttBrokerPort) );
  }

  mqttRxQueue = xQueueCreate(6, sizeof(MqttRxMessage));
  if (!mqttRxQueue) {
    Serial.println("MqttClient: MQTT RX queue creation failed!: MQTT broker IP: " + String(params->mqttBrokerIP) + ", port: " + String(params->mqttBrokerPort) );
  }
}

/* ###################################################################################################
 *                  M Q T T   E N Q U E U E   P U B L I S H
 * ###################################################################################################
 */
bool mqttEnqueuePublish(const char* topic, const char* payload, bool retain) {
  if (!mqttQueue) return false;

  MqttMessage msg{};
  strncpy(msg.topic, topic, MQTT_TOPIC_LEN - 1);
  strncpy(msg.payload, payload, MQTT_PAYLOAD_LEN - 1);
  msg.retain = retain;

  return xQueueSend(mqttQueue, &msg, 0) == pdTRUE;
}

bool publishMqttLog(const char* topicSuffix, const char* message, bool retain) {
  if (!topicSuffix || !message || !mqttQueue) {
    return false;
  }

  String topic = String(MQTT_PREFIX + mqttDeviceNameWithMac);
  if (topicSuffix[0] == '/') {
    topic += topicSuffix;
  } else {
    topic += "/";
    topic += topicSuffix;
  }

  char timestamp[32] = {0};
  formatLogTimestamp(timestamp, sizeof(timestamp));

  char payload[MQTT_PAYLOAD_LEN] = {0};
  snprintf(payload, sizeof(payload), "%s - %s", timestamp, message);

  return mqttEnqueuePublish(topic.c_str(), payload, retain);
}

bool publishMqttLogStatus(const char* message, bool retain) {
  return publishMqttLog(MQTT_LOG_STATUS_SUFFIX.c_str(), message, retain);
}

bool publishMqttLogEmail(const char* message, bool retain) {
  return publishMqttLog(MQTT_LOG_EMAIL_SUFFIX.c_str(), message, retain);
}

/* ###################################################################################################
 *                  M Q T T   L O O P
 * ###################################################################################################
 */
void mqttLoop(TaskParams_t* params) {
  if (mqttPaused) {
    return;  // Skip all MQTT operations when paused
  }
  
  if (!mqttClient.connected()) {
    reconnect(params);
  }

  mqttClient.loop();

  // Process outgoing messages
  MqttMessage msg;
  while (xQueueReceive(mqttQueue, &msg, 0) == pdTRUE) {

                                          #ifdef DEBUG
                                          Serial.println("MqttClient: Publishing to topic: " + String(msg.topic) + " payload: " + String(msg.payload) + " retain: " + String(msg.retain) );
                                          #endif

    mqttClient.publish(msg.topic, msg.payload, msg.retain);
  }
}

/* ###################################################################################################
 *                  M Q T T   I S   C O N N E C T E D
 * ###################################################################################################
 */
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

/* ###################################################################################################
 *                  I N I T I A L I Z E   M Q T T   G L O B A L S
 * ###################################################################################################
 *  
 */
void initializeMQTTGlobals()
{
  // >>>>>>>>>>>>>   Set globals for MQTT Device and Client   <<<<<<<<<<<<<<<<<<
  uint8_t mac[6];
  WiFi.macAddress(mac);

  char macStr[13] = {0};
  sprintf(macStr, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  mqttDeviceNameWithMac = String(MQTT_DEVICE_NAME + macStr);

  // MQTT 3.1 brokers often enforce a 23-char clientId limit.
  // Build a shorter clientId to avoid connection refusal (rc=2).
  String macShort = String(macStr).substring(6); // last 6 hex chars
  mqttClientWithMac = String(MQTT_CLIENT + macShort);
  if (mqttClientWithMac.length() > 23) {
    mqttClientWithMac = mqttClientWithMac.substring(0, 23);
  }
}

/* ###################################################################################################
 *                  M Q T T   C A L L B A C K
 * ###################################################################################################
 *  This function is called, when a subscribed topic receives a message.
 */
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  if (!mqttRxQueue || !topic || !payload || length == 0) {
    return;
  }

  MqttRxMessage msg{};
  strncpy(msg.topic, topic, MQTT_TOPIC_LEN - 1);

  if (length >= MQTT_PAYLOAD_LEN) {
    length = MQTT_PAYLOAD_LEN - 1;
  }
  memcpy(msg.payload, payload, length);
  msg.payload[length] = '\0';
  msg.length = static_cast<uint16_t>(length);

  xQueueSend(mqttRxQueue, &msg, 0);
}

void mqttProcessRxQueue() {
  if (!mqttRxQueue) {
    return;
  }

  MqttRxMessage msg;
  while (xQueueReceive(mqttRxQueue, &msg, 0) == pdTRUE) {
    JsonDocument doc;
    String topicString = String(msg.topic);

                                                                    #ifdef DEBUG
                                                                    Serial.print("MqttClient: Message arrived [");
                                                                    Serial.print(topicString);
                                                                    Serial.print("] Payload: ");
                                                                    Serial.println(msg.payload);
                                                                    #endif
    if (topicString.startsWith(MQTT_PREFIX) && topicString.endsWith(MQTT_SUFFIX_SET)) {
      DeserializationError error = deserializeJson(doc, msg.payload, msg.length);
      if (error) {
                                                                    #ifdef DEBUG
                                                                    Serial.print("MqttClient: JSON deserialization failed: ");
                                                                    Serial.println(error.c_str());
                                                                    #endif
        continue;
      }

      for (JsonPair kv : doc.as<JsonObject>()) {
        String key = kv.key().c_str();
        String value = kv.value().as<String>();

                                                                    #ifdef DEBUG
                                                                    Serial.print("MqttClient: Processing command - Key: ");
                                                                    Serial.print(key);
                                                                    Serial.print(", Value: ");
                                                                    Serial.println(value);
                                                                    #endif

        if (key == MQTT_NUMBER_ENERGY_ENTITYNAME) {
          float newEnergyValue = value.toFloat();
                                                                    #ifdef DEBUG
                                                                    Serial.print("MqttClient: Setting new energy value to: ");
                                                                    Serial.println(newEnergyValue);
                                                                    #endif

          if (mqttParams != nullptr) {
            uint32_t newPulseCounter = (uint32_t)(newEnergyValue * mqttParams->pulse_per_kWh + 0.5f);
            setPulseCounterFromMqtt(newPulseCounter);
          }
        } else if (key == MQTT_SENSOR_ENERGY_ENTITYNAME) {
          if (value.equalsIgnoreCase("true")) {
            requestSubtotalReset();
          }
        }
      }
    }
  }
}

/* ###################################################################################################
 *                  M Q T T   P A U S E   A N D   R E S U M E
 * ###################################################################################################
 *  These functions can be used to pause and resume MQTT operations.
 *  When paused, the MQTT client will disconnect and no messages will be sent or received.
 *  This is useful when the network is unstable or when performing critical operations that should
 *  not be interrupted by MQTT activities.
 */
void mqttPause() {
  mqttPaused = true;
  if (mqttClient.connected()) {
    mqttClient.disconnect();
  }
}

/* ###################################################################################################
 *                  M Q T T   R E S U M E
 * ###################################################################################################
 */
void mqttResume() {
  mqttPaused = false;
}

/*
 * ###################################################################################################
 *              P U B L I S H   M Q T T   E N E R G Y   C O N F I G U R A T I O N
 * ###################################################################################################
 * 
 * component can take the values: "sensor" or "number"
 * device_class can take the values "energy" or "power"
*/
void publishMqttEnergyConfigJson( String component, String entityName, String unitOfMeasurement, String deviceClass)
{
  char payload[1024];
  JsonDocument doc;

  if ( component == MQTT_NUMBER_COMPONENT & deviceClass == MQTT_ENERGY_DEVICECLASS)
  {
    doc["command_topic"] = String(MQTT_PREFIX + mqttDeviceNameWithMac + MQTT_SUFFIX_SET);
    doc["command_template"] = String("{\"" + entityName + "\": {{ value }} }");
    doc["max"] = 99999.99;
    doc["min"] = 0.0;
    doc["step"] = 0.01;
  }
  doc["name"] = entityName;
  doc["state_topic"] = String(MQTT_DISCOVERY_PREFIX + MQTT_PREFIX + MQTT_SUFFIX_STATE);
  doc["availability_topic"] = String(MQTT_PREFIX + mqttDeviceNameWithMac + MQTT_ONLINE);
  doc["payload_available"] = "True";
  doc["payload_not_available"] = "False";
  doc["device_class"] = deviceClass;
  doc["unit_of_measurement"] = unitOfMeasurement;
  doc["unique_id"] = String(entityName + "_" + mqttDeviceNameWithMac);
  doc["qos"] = 0;

  if ( component == MQTT_SENSOR_COMPONENT & deviceClass == MQTT_POWER_DEVICECLASS)
    doc["value_template"] = String("{{ value_json." + entityName + "}}");
  else 
    doc["value_template"] = String("{{ value_json." + entityName + " | round(2)}}");

  // Suggested by GPT-5.2
  /* 
  JsonObject device = doc.createNestedObject("device");
  JsonArray identifiers = device.createNestedArray("identifiers");
  identifiers.add(String("meter_0"));
  device["name"] = String("Energi - EV ESP32 Monitor");

  Previously used code for device object
  */
  JsonObject device = doc["device"].to<JsonObject>();

  device["identifiers"][0] = String("Meter");
  device["name"] = String("Energi - EV ESP32 Monitor");

  serializeJson(doc, payload, sizeof(payload));
  String energyTopic = String( MQTT_DISCOVERY_PREFIX + component + "/" + deviceClass + "/config");

  mqttEnqueuePublish(energyTopic.c_str(), payload, RETAINED);

}

/*
 * ###################################################################################################
 *                       P U B L I S H   M Q T T   C O N F I G U R A T I O N S  
 * ###################################################################################################
*/
void publishMqttConfigurations() {

  publishMqttEnergyConfigJson(MQTT_SENSOR_COMPONENT, MQTT_SENSOR_ENERGY_ENTITYNAME, "kWh", MQTT_ENERGY_DEVICECLASS);
  publishMqttEnergyConfigJson(MQTT_SENSOR_COMPONENT, MQTT_SENSOR_POWER_ENTITYNAME, "W", MQTT_POWER_DEVICECLASS);
  publishMqttEnergyConfigJson(MQTT_NUMBER_COMPONENT, MQTT_NUMBER_ENERGY_ENTITYNAME, "kWh", MQTT_ENERGY_DEVICECLASS);

  float powerW = 0.0f;
  float energyKwh = 0.0f;
  float subtotalKwh = 0.0f;
  if (getLatestEnergySnapshot(&powerW, &energyKwh, &subtotalKwh)) {
    publishMqttEnergy(powerW, energyKwh, subtotalKwh);
  }

  //configurationPublished[device] = true;
}

/* ###################################################################################################
 *                  P U B L I S H   M Q T T   E N E R G Y
 * ###################################################################################################
*/
void publishMqttEnergy(float powerW, float pulseCounter, float subtotalPulseCounter)
{
  if (!mqttClient.connected()) {
    return; // Exit if MQTT is not connected
  }
  
  char payload[256];
  JsonDocument doc;

  doc[MQTT_SENSOR_POWER_ENTITYNAME] = powerW;
  doc[MQTT_NUMBER_ENERGY_ENTITYNAME] = (float)pulseCounter;
  doc[MQTT_SENSOR_ENERGY_ENTITYNAME] = (float)subtotalPulseCounter;

  serializeJson(doc, payload, sizeof(payload));
  String energyTopic = String( MQTT_DISCOVERY_PREFIX + MQTT_PREFIX + MQTT_SUFFIX_STATE);

  mqttEnqueuePublish(energyTopic.c_str(), payload, RETAINED);
} 