#pragma once

#define MQTT_TOPIC_LEN   128
#define MQTT_PAYLOAD_LEN 1024

struct MqttMessage {
  char topic[MQTT_TOPIC_LEN];
  char payload[MQTT_PAYLOAD_LEN];
  bool retain;
};
