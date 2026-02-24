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

void updateTask(void* /*pvParameters*/) {
  for (;;) {
    OledLibrary::update();
    vTaskDelay(pdMS_TO_TICKS(updateIntervalMs > 0 ? updateIntervalMs : 1));
  }
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
    return true;
  }

  updateIntervalMs = intervalMs > 0 ? intervalMs : 1;

  const BaseType_t result = xTaskCreatePinnedToCore(updateTask,
                                                     "OledUpdateTask",
                                                     stackSizeWords,
                                                     nullptr,
                                                     priority,
                                                     &updateTaskHandle,
                                                     coreId);
  if (result != pdPASS) {
    updateTaskHandle = nullptr;
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
  vTaskDelete(updateTaskHandle);
  updateTaskHandle = nullptr;
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
