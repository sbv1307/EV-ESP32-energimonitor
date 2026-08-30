#include "LedTask.h"
#include "config.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <string.h>
#include <stdlib.h>

// ---------------------------------------------------------------------------
//  Timing constants
// ---------------------------------------------------------------------------

// Duration of each individual toggle in a Blink sequence (ms).
constexpr uint32_t LED_BLINK_ON_MS    = 100;

// Pause between successive blinks in a multi-blink sequence (ms).
constexpr uint32_t LED_BLINK_GAP_MS   = 100;

// Half-period used in Toggle mode, giving a ~1 Hz blink (ms).
constexpr uint32_t LED_TOGGLE_HALF_MS = 500;

// ---------------------------------------------------------------------------
//  Task / queue configuration
// ---------------------------------------------------------------------------

constexpr uint32_t LED_TASK_STACK_SIZE = 1536;  // words
constexpr UBaseType_t LED_TASK_PRIORITY = 1;
constexpr UBaseType_t LED_QUEUE_DEPTH   = 8;
constexpr size_t   LED_CMD_MAX_LEN      = 12;   // "Blink" (5) + up to 6 digits + NUL

// ---------------------------------------------------------------------------
//  Internal types
// ---------------------------------------------------------------------------

typedef struct {
    char cmd[LED_CMD_MAX_LEN];
} LedCommand_t;

// ---------------------------------------------------------------------------
//  Module-level state
// ---------------------------------------------------------------------------

struct LedInstance {
    uint8_t      gpio;
    QueueHandle_t queue      = nullptr;
    TaskHandle_t  taskHandle = nullptr;
};

static LedInstance sLeds[2] = {
    { static_cast<uint8_t>(LED_STATUS_GPIO) },
    { static_cast<uint8_t>(LED_CHARGE_GPIO) },
};

// ---------------------------------------------------------------------------
//  LED task body
// ---------------------------------------------------------------------------

static void ledTask(void* pvParams) {
    LedInstance* inst = static_cast<LedInstance*>(pvParams);
    const uint8_t gpio = inst->gpio;

    pinMode(gpio, OUTPUT);
    digitalWrite(gpio, LOW);
    bool ledState = false;   // tracks the current logical LED state

    LedCommand_t item;

    while (true) {
        // Wait indefinitely for the next command.
        if (xQueueReceive(inst->queue, &item, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        // ---------------------------------------------------------------
        //  Toggle mode – blink continuously at ~1 Hz until a new (different)
        //  command arrives.  xQueueReceive is used as a timed wait; every
        //  LED_TOGGLE_HALF_MS milliseconds without a new command the LED
        //  changes state.  When a new command is received early, item.cmd
        //  is updated and the while-condition re-evaluates it.
        // ---------------------------------------------------------------
        while (strcmp(item.cmd, "Toggle") == 0) {
            if (xQueueReceive(inst->queue, &item, pdMS_TO_TICKS(LED_TOGGLE_HALF_MS)) != pdTRUE) {
                // Timeout – half period elapsed: flip LED.
                ledState = !ledState;
                digitalWrite(gpio, ledState ? HIGH : LOW);
            }
            // If a command was received, the while condition is re-checked.
            // A non-Toggle command exits the loop; another "Toggle" keeps it going.
        }

        // ---------------------------------------------------------------
        //  Blink[N] – toggle LED away from its current state for a short
        //  period, then toggle back.  Repeats N times (default 1).
        //  Works with the LED in any initial state: ON → brief OFF, or
        //  OFF → brief ON.
        // ---------------------------------------------------------------
        if (strncmp(item.cmd, "Blink", 5) == 0) {
            int count = 1;
            if (item.cmd[5] != '\0') {
                int n = atoi(item.cmd + 5);
                if (n >= 1) count = n;
            }

            for (int i = 0; i < count; i++) {
                // Toggle away from current state.
                ledState = !ledState;
                digitalWrite(gpio, ledState ? HIGH : LOW);
                vTaskDelay(pdMS_TO_TICKS(LED_BLINK_ON_MS));

                // Toggle back.
                ledState = !ledState;
                digitalWrite(gpio, ledState ? HIGH : LOW);

                // Inter-blink gap (omitted after the last blink).
                if (i < count - 1) {
                    vTaskDelay(pdMS_TO_TICKS(LED_BLINK_GAP_MS));
                }
            }
        }

        // ---------------------------------------------------------------
        //  TurnOn / TurnOff – steady state.
        // ---------------------------------------------------------------
        else if (strcmp(item.cmd, "TurnOn") == 0) {
            ledState = true;
            digitalWrite(gpio, HIGH);
        }
        else if (strcmp(item.cmd, "TurnOff") == 0) {
            ledState = false;
            digitalWrite(gpio, LOW);
        }
        // Unknown commands are silently ignored.
    }

    vTaskDelete(nullptr);
}

// ---------------------------------------------------------------------------
//  Public API
// ---------------------------------------------------------------------------

void sendLedCommand(LedId id, const char* command) {
    if (!command) return;

    LedInstance& inst = sLeds[static_cast<uint8_t>(id)];

    // Lazily create the queue and start the task on the very first call.
    if (inst.queue == nullptr) {
        inst.queue = xQueueCreate(LED_QUEUE_DEPTH, sizeof(LedCommand_t));
    }
    if (inst.taskHandle == nullptr || eTaskGetState(inst.taskHandle) == eDeleted) {
        xTaskCreate(ledTask, id == LedId::Status ? "LedTaskStatus" : "LedTaskCharge",
                    LED_TASK_STACK_SIZE, &inst,
                    LED_TASK_PRIORITY, &inst.taskHandle);
    }

    LedCommand_t item;
    strncpy(item.cmd, command, LED_CMD_MAX_LEN - 1);
    item.cmd[LED_CMD_MAX_LEN - 1] = '\0';

    // Non-blocking send; if the queue is full the command is dropped rather
    // than blocking the caller (which may be called from a time-sensitive context).
    xQueueSend(inst.queue, &item, 0);
}
