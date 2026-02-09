#include "PulseInputIsrTest.h"

#include <Arduino.h>

#include "PulseInputTask.h"
#include "MqttClient.h"

static TaskHandle_t PulseInputIsrTestTaskHandle = nullptr;

static void PulseInputIsrTestTask(void* pvParameters) {
  const TickType_t intervalTicks = (TickType_t)((uint64_t)4UL * 60UL * 60UL * (uint64_t)configTICK_RATE_HZ); // 6 times per day
  TickType_t lastWakeTick = xTaskGetTickCount();
  uint32_t lastNotReadyLogMs = 0;
  bool wasReady = false;

  while (true) {
    while (!isPulseInputReady()) {
      uint32_t nowMs = millis();
      if (nowMs - lastNotReadyLogMs >= 60000U) {
        publishMqttLogStatus("PulseInputIsrTestTask waiting for pulse input ready", false);
        lastNotReadyLogMs = nowMs;
      }
      wasReady = false;
      vTaskDelay(pdMS_TO_TICKS(1000));
      lastWakeTick = xTaskGetTickCount();
    }

    if (!wasReady) {
      publishMqttLogStatus("PulseInputIsrTestTask ready", false);
      wasReady = true;
    }

    vTaskDelayUntil(&lastWakeTick, intervalTicks);
    PulseInputISR();
    publishMqttLog(MQTT_LOG_SUFFIX.c_str(), "PulseInputIsrTestTask simulated pulse", false);
  }
}

void startPulseInputIsrTestTask() {
  if (PulseInputIsrTestTaskHandle != nullptr && eTaskGetState(PulseInputIsrTestTaskHandle) != eDeleted) {
    return;
  }

  publishMqttLogStatus("PulseInputIsrTestTask starting", false);
  xTaskCreate(
    PulseInputIsrTestTask,
    "PulseISRTestTask",
    PULSE_ISR_TEST_TASK_STACK_SIZE,
    nullptr,
    1,
    &PulseInputIsrTestTaskHandle
  );
}
