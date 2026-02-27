#define DEBUG
//#define STACK_WATERMARK

#include "PulseInputTask.h"
#include "MqttClient.h"
#include "TeslaSheets.h"
#include "config.h"

#include <Preferences.h>

#define SAVE_INTERVAL_MS 60000  // Save to NVS every 60 seconds

static TaskHandle_t PulseInputTaskHandle = nullptr;
static QueueHandle_t PulseInputQueue = nullptr;
static volatile bool PulseInputTaskReady = false;
static portMUX_TYPE PulseCounterMux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool PulseCounterUpdatePending = false;
static volatile uint32_t PendingPulseCounter = 0;
static portMUX_TYPE EnergyKwhMux = portMUX_INITIALIZER_UNLOCKED;
static volatile float LatestEnergyKwh = 0.0f;
static volatile float LatestPowerW = 0.0f;
static volatile float LatestSubtotalKwh = 0.0f;
static portMUX_TYPE SubtotalResetMux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool SubtotalResetPending = false;

void setPulseCounterFromMqtt(uint32_t newPulseCounter) {
  portENTER_CRITICAL(&PulseCounterMux);
  PendingPulseCounter = newPulseCounter;
  PulseCounterUpdatePending = true;
  portEXIT_CRITICAL(&PulseCounterMux);
}

void requestSubtotalReset() {
  portENTER_CRITICAL(&SubtotalResetMux);
  SubtotalResetPending = true;
  portEXIT_CRITICAL(&SubtotalResetMux);

  publishMqttLog(MQTT_LOG_SUFFIX, "Subtotal reset requested", false);
}

bool getLatestEnergyKwh(float* energyKwh) {
  if (!energyKwh) {
    return false;
  }

  portENTER_CRITICAL(&EnergyKwhMux);
  *energyKwh = LatestEnergyKwh;
  portEXIT_CRITICAL(&EnergyKwhMux);
  return true;
}

bool getLatestEnergySnapshot(float* powerW, float* energyKwh, float* subtotalKwh) {
  if (!powerW || !energyKwh || !subtotalKwh) {
    return false;
  }

  portENTER_CRITICAL(&EnergyKwhMux);
  *powerW = LatestPowerW;
  *energyKwh = LatestEnergyKwh;
  *subtotalKwh = LatestSubtotalKwh;
  portEXIT_CRITICAL(&EnergyKwhMux);
  return true;
}

/* ###################################################################################################
*               N V S   H A N D L I N G    L O A D  F R O M
 * ###################################################################################################
 */
uint32_t loadFromNVS(uint16_t* subtotalPulseCounter) {
  Preferences pref;
  pref.begin(COUNT_NVS_NAMESPACE, true); // true = read-only
  uint32_t pulseCounter = pref.getUInt("pulse_count", 0);
  uint32_t subtotalStored = pref.getUInt("subtotal_count", 0);
  pref.end();
  if (subtotalPulseCounter != nullptr) {
    *subtotalPulseCounter = (uint16_t)subtotalStored;
  }
  return pulseCounter;
}

/* ###################################################################################################
 *               N V S   H A N D L I N G    S A V E    T O
 * ###################################################################################################
 */
void saveToNVS(uint32_t pulseCounter, uint16_t subtotalPulseCounter) {
  Preferences pref;
  pref.begin(COUNT_NVS_NAMESPACE, false); // false = read/write
  pref.putUInt("pulse_count", pulseCounter);
  pref.putUInt("subtotal_count", (uint32_t)subtotalPulseCounter);
  pref.end();
}

/* ###################################################################################################
 *               P O W E R    C A L C U L A T I O N
 * ###################################################################################################
 */
float calculatePower( TaskParams_t* params,uint32_t deltaUs) {
  // Power calculation logic here
  float powerW = 0.0f;
  if (deltaUs > 0) {
      powerW = (3600.0f * 1000000.0f) / deltaUs; // Example calculation
    powerW = round(((float)(60*60*1000) / 
                    (float)(deltaUs +  params->ptCorrection)) / 
                    (float)params->pulse_per_kWh * 1000);
  }
  return powerW;
}

/* ###################################################################################################
 *               P U L S E    I N P U T    I S R
 * ###################################################################################################
 */
void IRAM_ATTR PulseInputISR() {
  if (PulseInputQueue == nullptr) {
    return;
  }
  unsigned long ts = micros();
  BaseType_t higherPriorityTaskWoken = pdFALSE;
  xQueueSendFromISR(PulseInputQueue, &ts, &higherPriorityTaskWoken);
  portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

/* ###################################################################################################
 *             I S   P U L S E    I N P U T   R E A D Y
 * ###################################################################################################
 */
bool isPulseInputReady() {
  return PulseInputQueue != nullptr && PulseInputTaskReady;
}

bool waitForPulseInputReady(uint32_t timeoutMs) {
  uint32_t startMs = millis();
  while (!isPulseInputReady()) {
    if (timeoutMs > 0 && (millis() - startMs) >= timeoutMs) {
      return false;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  return true;
}

bool attachPulseInputInterrupt(int gpio, int mode) {
  if (gpio < 0 || PulseInputQueue == nullptr) {
    return false;
  }

  pinMode(gpio, INPUT);
  attachInterrupt(digitalPinToInterrupt(gpio), PulseInputISR, mode);
  return true;
}

/* ###################################################################################################
 * ###################################################################################################
 * ###################################################################################################
 *               P U L S E    I N P U T    T A S K
 * ###################################################################################################
 * ###################################################################################################
 * ###################################################################################################
 */
static void PulseInputTask( void* pvParameters) {
  // Task initialization
  uint32_t ts;
  uint32_t lastTs = 0;
  uint16_t subtotalPulseCounter = 0;
  uint32_t pulseCounter = loadFromNVS(&subtotalPulseCounter);
  float powerW = 0.0f;
  float energyKwh = (float)pulseCounter / (float)((TaskParams_t*)pvParameters)->pulse_per_kWh;
  float subtotalKwh = (float)subtotalPulseCounter / (float)((TaskParams_t*)pvParameters)->pulse_per_kWh;

  portENTER_CRITICAL(&EnergyKwhMux);
  LatestPowerW = powerW;
  LatestEnergyKwh = energyKwh;
  LatestSubtotalKwh = subtotalKwh;
  portEXIT_CRITICAL(&EnergyKwhMux);


  uint32_t lastSaveMs = millis();
  uint32_t lastSavedPulseCounter = pulseCounter;
  uint16_t lastSavedSubtotalPulseCounter = subtotalPulseCounter;

                                                    #ifdef DEBUG
                                                    Serial.println("Pulse Input Task initializing...\n");
                                                    #endif

  if (PulseInputQueue == nullptr) {

                                                    #ifdef DEBUG
                                                    Serial.println("Pulse count queue not initialized!");
                                                    #endif
    
    PulseInputTaskHandle = nullptr;
    vTaskDelete(nullptr);
    return;
  }

  PulseInputTaskReady = true;
 
                                                    #ifdef DEBUG
                                                    Serial.println("Pulse Input Task started with following parameters:\n");
                                                    Serial.println("pulse counter loaded from NVS: " + String(pulseCounter));

                                                    Serial.println("Pulse Time Correction: " + String(((TaskParams_t*)pvParameters)->ptCorrection));
                                                    Serial.println("Pulses per kWh: " + String(((TaskParams_t*)pvParameters)->pulse_per_kWh));
                                                    Serial.println("-----------------------------------------------------\n");
                                                    Serial.print ("\nWaiting for pulses");
                                                    #endif

  // Main task loop
  while (true) {
    if (PulseCounterUpdatePending) {
      portENTER_CRITICAL(&PulseCounterMux);
      uint32_t previousPulseCounter = pulseCounter;
      pulseCounter = PendingPulseCounter;
      PulseCounterUpdatePending = false;
      portEXIT_CRITICAL(&PulseCounterMux);

      float energyKwh = (float)pulseCounter / (float)((TaskParams_t*)pvParameters)->pulse_per_kWh;
      float subtotalKwh = (float)subtotalPulseCounter / (float)((TaskParams_t*)pvParameters)->pulse_per_kWh;

      portENTER_CRITICAL(&EnergyKwhMux);
      LatestPowerW = powerW;
      LatestEnergyKwh = energyKwh;
      LatestSubtotalKwh = subtotalKwh;
      portEXIT_CRITICAL(&EnergyKwhMux);
      publishMqttEnergy(0.0f, energyKwh, subtotalKwh);

      if (pulseCounter != previousPulseCounter) {
        saveToNVS(pulseCounter, subtotalPulseCounter);
        lastSavedPulseCounter = pulseCounter;
        lastSavedSubtotalPulseCounter = subtotalPulseCounter;
        lastSaveMs = millis();
      }
    }

    bool shouldResetSubtotal = false;
    portENTER_CRITICAL(&SubtotalResetMux);
    if (SubtotalResetPending) {
      SubtotalResetPending = false;
      shouldResetSubtotal = true;
    }
    portEXIT_CRITICAL(&SubtotalResetMux);

    if (shouldResetSubtotal) {

      char resetMsg[128] = {0};
      snprintf(resetMsg,
               sizeof(resetMsg),
               "Subtotal reset handling: total=%lu subtotal=%u",
               (unsigned long)pulseCounter,
               (unsigned)subtotalPulseCounter);
      publishMqttLogStatus(resetMsg, false);

      bool subtotalChanged = subtotalPulseCounter != 0;
      subtotalPulseCounter = 0;
      if (subtotalChanged) {
        saveToNVS(pulseCounter, subtotalPulseCounter);
        lastSavedPulseCounter = pulseCounter;
        lastSavedSubtotalPulseCounter = subtotalPulseCounter;
        lastSaveMs = millis();
      }

      float energyKwh = (float)pulseCounter / (float)((TaskParams_t*)pvParameters)->pulse_per_kWh;
      float subtotalKwh = 0.0f;
      portENTER_CRITICAL(&EnergyKwhMux);
      LatestPowerW = powerW;
      LatestEnergyKwh = energyKwh;
      LatestSubtotalKwh = subtotalKwh;
      portEXIT_CRITICAL(&EnergyKwhMux);
      publishMqttEnergy(0.0f, energyKwh, subtotalKwh);

      publishMqttLog(MQTT_LOG_SUFFIX, "Subtotal reset applied", false);
    }

    if (xQueueReceive(PulseInputQueue, &ts, pdMS_TO_TICKS(1000))) {

                                          #ifdef DEBUG
                                          Serial.println("\nProcessing pulse count and timestamp: " + String(ts) + "\n");
                                          #endif

      // ---- 1. Pulse counting ----
      pulseCounter++;
      subtotalPulseCounter++;

                                          #ifdef DEBUG
                                          Serial.println("\nPulse Count: " + String(pulseCounter) + " Subtotal Pulse Count: " + String(subtotalPulseCounter));
                                          #endif

      // ---- 2. Power calculation ----
      if (lastTs > 0) {
          uint32_t deltaUs = ts - lastTs;
          powerW = calculatePower( (TaskParams_t*)pvParameters, deltaUs);

                                                          #ifdef DEBUG
                                                            Serial.println("\nDelta U sec: " + String(deltaUs) + " Pulse Count: " + String(pulseCounter) + " Power: " + String(powerW) + " W");
                                                          #endif

      }
      lastTs = ts;

      float energyKwh = (float)pulseCounter / (float)((TaskParams_t*)pvParameters)->pulse_per_kWh;
      float subtotalKwh = (float)subtotalPulseCounter / (float)((TaskParams_t*)pvParameters)->pulse_per_kWh;
      publishMqttEnergy(powerW, energyKwh, subtotalKwh);

      portENTER_CRITICAL(&EnergyKwhMux);
      LatestPowerW = powerW;
      LatestEnergyKwh = energyKwh;
      LatestSubtotalKwh = subtotalKwh;
      portEXIT_CRITICAL(&EnergyKwhMux);

    }


                                                          #ifdef DEBUG
                                                            Serial.print (".");
                                                          #endif  

    // ---- 3. Periodic NVS save ----
    if (millis() - lastSaveMs >= SAVE_INTERVAL_MS) {
      bool hasCounterChanges = (pulseCounter != lastSavedPulseCounter) ||
                               (subtotalPulseCounter != lastSavedSubtotalPulseCounter);

      if (hasCounterChanges) {

                                          #ifdef DEBUG
                                          Serial.println("\nSaving pulse count to NVS: " + String(pulseCounter) + "\n");
                                          #endif

        saveToNVS(pulseCounter, subtotalPulseCounter);
        lastSavedPulseCounter = pulseCounter;
        lastSavedSubtotalPulseCounter = subtotalPulseCounter;
        lastSaveMs = millis();
      }
    }

                                                            #ifdef STACK_WATERMARK
                                                            static uint32_t lastLog = 0;
                                                            if (millis() - lastLog > 5000) {
                                                              lastLog = millis();
                                                              gPulseInputTaskStackHighWater = uxTaskGetStackHighWaterMark(nullptr);
                                                            }
                                                            #endif
    
  

  }

}

/* ###################################################################################################
 *               S T A R T   P U L S E   I N P U T   T A S K
 * ###################################################################################################
 */
void startPulseInputTask(TaskParams_t* params) {
  // Check if task handle exists and task is still running
  if (PulseInputTaskHandle != nullptr && eTaskGetState(PulseInputTaskHandle) != eDeleted) {
    return; // Task already running
  }

  PulseInputTaskReady = false;

  if (PulseInputQueue == nullptr) {
    PulseInputQueue = xQueueCreate(10, sizeof(unsigned long));
    if (!PulseInputQueue) {

                                                #ifdef DEBUG
                                                Serial.println("Pulse count queue creation failed!");
                                                #endif

      return;
    }
  }
  
  xTaskCreate(
    PulseInputTask,
    "PulseInputTask",
    PULSE_INPUT_TASK_STACK_SIZE,
    params,
    1,
    &PulseInputTaskHandle
  );
}

