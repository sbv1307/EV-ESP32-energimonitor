#include "PulseInputIsrTest.h"

#include <Arduino.h>

#include "PulseInputTask.h"

static TaskHandle_t PulseInputIsrTestTaskHandle = nullptr;

static void PulseInputIsrTestTask(void* pvParameters) {
  const TickType_t intervalTicks = pdMS_TO_TICKS(4UL * 60UL * 60UL * 1000UL); // 6 times per day
  TickType_t lastWakeTick = xTaskGetTickCount();

  while (true) {
    while (!isPulseInputReady()) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      lastWakeTick = xTaskGetTickCount();
    }

    vTaskDelayUntil(&lastWakeTick, intervalTicks);
    PulseInputISR();
  }
}

void startPulseInputIsrTestTask() {
  if (PulseInputIsrTestTaskHandle != nullptr && eTaskGetState(PulseInputIsrTestTaskHandle) != eDeleted) {
    return;
  }

  xTaskCreate(
    PulseInputIsrTestTask,
    "PulseISRTestTask",
    2048,
    nullptr,
    1,
    &PulseInputIsrTestTaskHandle
  );
}
