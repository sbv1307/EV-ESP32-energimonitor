#pragma once

#define MQTT_TOPIC_LEN   64
#define MQTT_PAYLOAD_LEN 1024

struct MqttMessage {
  char topic[MQTT_TOPIC_LEN];
  char payload[MQTT_PAYLOAD_LEN];
  bool retain;
};
