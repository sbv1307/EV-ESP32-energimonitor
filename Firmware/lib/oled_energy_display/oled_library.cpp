#include "oled_library.h"

/*
 *  ARDUINO_ARCH_ESP32 is not defined in the project source:
 * it’s injected as a compiler define by the ESP32 Arduino build environment in PlatformIO.
 * - It can seen in the generated build metadata under the defines list: idedata.json:1
 * - That define comes from the ESP32 Arduino platform/toolchain configuration used by the
 *   selected environment/board, not from a #define in the code.
*/
#if defined(ARDUINO_ARCH_ESP32)
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace {
#if defined(ARDUINO_ARCH_ESP32)
TaskHandle_t updateTaskHandle = nullptr;
uint32_t updateIntervalMs = 20;
volatile bool updateTaskRunRequested = false;

void updateTask(void* /*pvParameters*/) {
  for (;;) {
    if (!updateTaskRunRequested) {
      break;
    }

    OledLibrary::update();

    if (!updateTaskRunRequested) {
      break;
    }

    vTaskDelay(pdMS_TO_TICKS(updateIntervalMs > 0 ? updateIntervalMs : 1));
  }

  updateTaskHandle = nullptr;
  vTaskDelete(nullptr);
}
#endif
}

namespace OledLibrary {
bool begin() {
  return begin(Settings{});
}

bool begin(const Settings& settings) {
  if (!OledEnergyDisplay::begin(settings.energyDisplay)) {
    return false;
  }

  if (settings.showSplashOnBoot && settings.splashText != nullptr && settings.splashText[0] != '\0') {
    OledEnergyDisplay::showSplash(String(settings.splashText), settings.splashDurationMs);
    if (settings.turnOffAfterSplash) {
      OledEnergyDisplay::turnOff();
    }
  }

  OledTouchWake::begin(settings.touchWake);
  return true;
}

void update() {
  OledTouchWake::update();
  OledEnergyDisplay::update();
}

bool startBackgroundUpdater(uint32_t intervalMs,
                           uint32_t stackSizeWords,
                           uint32_t priority,
                           int8_t coreId) {
#if defined(ARDUINO_ARCH_ESP32)
  if (updateTaskHandle != nullptr) {
    updateTaskRunRequested = true;
    return true;
  }

  updateIntervalMs = intervalMs > 0 ? intervalMs : 1;
  updateTaskRunRequested = true;

  const BaseType_t result = xTaskCreatePinnedToCore(updateTask,
                                                     "OledUpdateTask",
                                                     stackSizeWords,
                                                     nullptr,
                                                     priority,
                                                     &updateTaskHandle,
                                                     coreId);
  if (result != pdPASS) {
    updateTaskHandle = nullptr;
    updateTaskRunRequested = false;
    return false;
  }

  return true;
#else
  (void)intervalMs;
  (void)stackSizeWords;
  (void)priority;
  (void)coreId;
  return false;
#endif
}

void stopBackgroundUpdater() {
#if defined(ARDUINO_ARCH_ESP32)
  if (updateTaskHandle == nullptr) {
    return;
  }

  // Stop cooperatively so the update task can release shared display mutexes.
  // Force-deleting the task while it owns a mutex can deadlock OTA callbacks.
  updateTaskRunRequested = false;

  const uint32_t startMs = millis();
  while (updateTaskHandle != nullptr && (millis() - startMs) < 500) {
    vTaskDelay(pdMS_TO_TICKS(1));
  }
#endif
}

bool isBackgroundUpdaterRunning() {
#if defined(ARDUINO_ARCH_ESP32)
  return updateTaskHandle != nullptr;
#else
  return false;
#endif
}
}
