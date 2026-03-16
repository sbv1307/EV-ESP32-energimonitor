#define STACK_WATERMARK

#include "PushButtonTask.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_timer.h>

#include "config.h"
#include "globals.h"
#include "ChargingSession.h"
#include "MqttClient.h"

// ---------------------------------------------------------------------------
//  Command enum – one value per physical button action.
// ---------------------------------------------------------------------------
typedef enum : uint8_t {
  BTN_CMD_EV_CHARGING_TOGGLE    = 0,
  BTN_CMD_SMART_CHARGING_TOGGLE = 1,
  BTN_CMD_PRICE_LIMIT_INCREASE  = 2,
  BTN_CMD_PRICE_LIMIT_DECREASE  = 3,
} ButtonCommand;

static QueueHandle_t sButtonQueue = nullptr;

// ---------------------------------------------------------------------------
//  ISR handlers – one per button.
//  Hardware debounce: reject edges that arrive within BUTTON_DEBOUNCE_MS ms
//  of the previous accepted edge, using esp_timer_get_time() which is
//  documented as ISR-safe on ESP32.
// ---------------------------------------------------------------------------
static void IRAM_ATTR onEvChargingButton() {
  static volatile int64_t lastUs = 0;
  int64_t now = esp_timer_get_time();
  if (now - lastUs < (int64_t)BUTTON_DEBOUNCE_MS * 1000LL) return;
  lastUs = now;
  ButtonCommand cmd = BTN_CMD_EV_CHARGING_TOGGLE;
  BaseType_t woken = pdFALSE;
  xQueueSendFromISR(sButtonQueue, &cmd, &woken);
  if (woken) portYIELD_FROM_ISR();
}

static void IRAM_ATTR onSmartChargingButton() {
  static volatile int64_t lastUs = 0;
  int64_t now = esp_timer_get_time();
  if (now - lastUs < (int64_t)BUTTON_DEBOUNCE_MS * 1000LL) return;
  lastUs = now;
  ButtonCommand cmd = BTN_CMD_SMART_CHARGING_TOGGLE;
  BaseType_t woken = pdFALSE;
  xQueueSendFromISR(sButtonQueue, &cmd, &woken);
  if (woken) portYIELD_FROM_ISR();
}

static void IRAM_ATTR onPriceLimitIncreaseButton() {
  static volatile int64_t lastUs = 0;
  int64_t now = esp_timer_get_time();
  if (now - lastUs < (int64_t)BUTTON_DEBOUNCE_MS * 1000LL) return;
  lastUs = now;
  ButtonCommand cmd = BTN_CMD_PRICE_LIMIT_INCREASE;
  BaseType_t woken = pdFALSE;
  xQueueSendFromISR(sButtonQueue, &cmd, &woken);
  if (woken) portYIELD_FROM_ISR();
}

static void IRAM_ATTR onPriceLimitDecreaseButton() {
  static volatile int64_t lastUs = 0;
  int64_t now = esp_timer_get_time();
  if (now - lastUs < (int64_t)BUTTON_DEBOUNCE_MS * 1000LL) return;
  lastUs = now;
  ButtonCommand cmd = BTN_CMD_PRICE_LIMIT_DECREASE;
  BaseType_t woken = pdFALSE;
  xQueueSendFromISR(sButtonQueue, &cmd, &woken);
  if (woken) portYIELD_FROM_ISR();
}

// ---------------------------------------------------------------------------
//  One-shot publish task.
//  Receives the ButtonCommand as a void* (uintptr_t cast).
//  Reads current global state, builds the JSON payload, publishes, and
//  deletes itself.
// ---------------------------------------------------------------------------
static void publishButtonCommandTask(void* param) {
  ButtonCommand cmd = (ButtonCommand)(uintptr_t)param;
  char payload[64];

  switch (cmd) {
    case BTN_CMD_EV_CHARGING_TOGGLE:
      snprintf(payload, sizeof(payload),
               "{\"%s\":\"%s\"}",
               BUTTON_EV_CHARGING,
               isChargingSessionCharging() ? "stop" : "start");
      break;

    case BTN_CMD_SMART_CHARGING_TOGGLE:
      snprintf(payload, sizeof(payload),
               "{\"%s\":\"%s\"}",
               BUTTON_SMART_CHARGING_ACTIVATED,
               gSmartChargingActivated ? "off" : "on");
      break;

    case BTN_CMD_PRICE_LIMIT_INCREASE:
      snprintf(payload, sizeof(payload),
               "{\"%s\":%.3f}",
               BUTTON_PRICE_LIMIT,
               gEnergyPriceLimit + BUTTON_PRICE_LIMIT_STEP);
      break;

    case BTN_CMD_PRICE_LIMIT_DECREASE: {
      float newLimit = gEnergyPriceLimit - BUTTON_PRICE_LIMIT_STEP;
      if (newLimit < 0.0f) newLimit = 0.0f;
      snprintf(payload, sizeof(payload), "{\"%s\":%.3f}", BUTTON_PRICE_LIMIT, newLimit);
      break;
    }

    default:
      vTaskDelete(nullptr);
      return;
  }

  publishMqttSetCommand(payload, false);

  #ifdef STACK_WATERMARK
  gButtonPublishTaskStackHighWater = uxTaskGetStackHighWaterMark(nullptr);
  #endif

  vTaskDelete(nullptr);
}

// ---------------------------------------------------------------------------
//  Public API
// ---------------------------------------------------------------------------

void initPushButtons() {
  sButtonQueue = xQueueCreate(8, sizeof(ButtonCommand));

  const int mode = BUTTON_ACTIVE_LOW ? FALLING : RISING;

  if (BUTTON_EV_CHARGING_TOGGLE_GPIO >= 0) {
    pinMode(BUTTON_EV_CHARGING_TOGGLE_GPIO, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_EV_CHARGING_TOGGLE_GPIO),
                    onEvChargingButton, mode);
  }
  if (BUTTON_SMART_CHARGING_TOGGLE_GPIO >= 0) {
    pinMode(BUTTON_SMART_CHARGING_TOGGLE_GPIO, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_SMART_CHARGING_TOGGLE_GPIO),
                    onSmartChargingButton, mode);
  }
  if (BUTTON_PRICE_LIMIT_INCREASE_GPIO >= 0) {
    pinMode(BUTTON_PRICE_LIMIT_INCREASE_GPIO, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PRICE_LIMIT_INCREASE_GPIO),
                    onPriceLimitIncreaseButton, mode);
  }
  if (BUTTON_PRICE_LIMIT_DECREASE_GPIO >= 0) {
    pinMode(BUTTON_PRICE_LIMIT_DECREASE_GPIO, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PRICE_LIMIT_DECREASE_GPIO),
                    onPriceLimitDecreaseButton, mode);
  }
}

void processPushButtonCommands() {
  if (!sButtonQueue) return;

  ButtonCommand cmd;
  while (xQueueReceive(sButtonQueue, &cmd, 0) == pdTRUE) {
    xTaskCreate(publishButtonCommandTask,
                "btnPublish",
                BUTTON_PUBLISH_TASK_STACK_SIZE,
                (void*)(uintptr_t)cmd,
                1,
                nullptr);
  }
}
