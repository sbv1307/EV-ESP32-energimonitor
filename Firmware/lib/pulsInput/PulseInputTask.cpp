//#define DEBUG
//#define HEADLESS_DEBUG
#define STACK_WATERMARK

#include "PulseInputTask.h"
#include "MqttClient.h"
#include "TeslaSheets.h"
#include "ChargingSession.h"
#include "config.h"
#include "LedTask.h"
#include "OtaService.h"
#include "oled_energy_display.h"

#include <Preferences.h>
#include <esp_system.h>

#define SAVE_INTERVAL_MS 60000  // Save to NVS every 60 seconds

// Direct-reset detection is enabled after the hardware fix.
#define ENABLE_DIRECT_RESET 1

static TaskHandle_t PulseInputTaskHandle = nullptr;
static QueueHandle_t PulseInputQueue = nullptr;
static volatile bool PulseInputTaskReady = false;
static portMUX_TYPE PulseCounterMux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool PulseCounterUpdatePending = false;
static volatile uint32_t PendingPulseCounter = 0;
static portMUX_TYPE PulseDiagnosticsMux = portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t PulseIsrEdges = 0;
static volatile uint32_t PulseQueuedEvents = 0;
static volatile uint32_t PulseDroppedEvents = 0;
static volatile uint32_t PulseProcessedEvents = 0;
static volatile uint32_t PulseTaskHeartbeats = 0;
// Marks the last reached point in the PulseInputTask loop, to pinpoint where the task stalls.
enum PulseInputStage_t : uint32_t {
  STAGE_LOOP_TOP = 1,
  STAGE_RESET_CHECK = 2,
  STAGE_PENDING_COUNTER = 3,
  STAGE_SUBTOTAL_RESET = 4,
  STAGE_COST_RESET = 5,
  STAGE_QUEUE_WAIT = 6,
  STAGE_PULSE_PROCESSED = 7,
  STAGE_POWER_DECAY_CHECK = 8,
  STAGE_PERIODIC_SAVE_START = 9,
  STAGE_PERIODIC_SAVE_DONE = 10,
  STAGE_LOOP_END = 11,
};
static volatile uint32_t PulseTaskStage = 0;
static volatile uint32_t DirectResetTriggerCount = 0;
static volatile bool DirectResetActive = false;

static inline void setPulseTaskStage(PulseInputStage_t stage) {
  PulseTaskStage = stage;
}
static uint16_t sPulsePerKwh = 0; // Initialized in startPulseInputTask() from TaskParams_t.
static portMUX_TYPE EnergyKwhMux = portMUX_INITIALIZER_UNLOCKED;
static volatile float LatestEnergyKwh = 0.0f;
static volatile float LatestPowerW = 0.0f;
static volatile float LatestSubtotalKwh = 0.0f;
static portMUX_TYPE CostMux = portMUX_INITIALIZER_UNLOCKED;
static volatile float LatestLastChargeCost = 0.0f;
static volatile float LatestDailyCost = 0.0f;
static volatile float LatestMonthlyCost = 0.0f;
static volatile float LatestQuarterlyCost = 0.0f;
static portMUX_TYPE SubtotalResetMux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool SubtotalResetPending = false;
static portMUX_TYPE CostResetMux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool LastChargeCostResetPending = false;
static volatile bool DailyCostResetPending = false;
static volatile bool MonthlyCostResetPending = false;
static volatile bool QuarterlyCostResetPending = false;

static portMUX_TYPE ResetMux = portMUX_INITIALIZER_UNLOCKED;
static volatile ResetType_t gResetType = RESET_SOFT;
static volatile bool gResetRequested = false;

static portMUX_TYPE EmergencyCounterMux = portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t gEmergencyPulseCounter = 0;
static volatile uint16_t gEmergencySubtotalPulseCounter = 0;
static volatile float gEmergencyLastChargeCost = 0.0f;
static volatile float gEmergencyDailyCost = 0.0f;
static volatile float gEmergencyMonthlyCost = 0.0f;
static volatile float gEmergencyQuarterlyCost = 0.0f;

#if ENABLE_DIRECT_RESET
static SemaphoreHandle_t sDirectResetSemaphore = nullptr;
#endif

static inline void updateLatestEnergySnapshot(float powerW, float energyKwh, float subtotalKwh) {
  portENTER_CRITICAL(&EnergyKwhMux);
  LatestPowerW = powerW;
  LatestEnergyKwh = energyKwh;
  LatestSubtotalKwh = subtotalKwh;
  portEXIT_CRITICAL(&EnergyKwhMux);
}

static inline void updateEmergencyCounters(uint32_t pulseCounter, uint16_t subtotalPulseCounter) {
  portENTER_CRITICAL(&EmergencyCounterMux);
  gEmergencyPulseCounter = pulseCounter;
  gEmergencySubtotalPulseCounter = subtotalPulseCounter;
  portEXIT_CRITICAL(&EmergencyCounterMux);
}

static inline void updateEmergencyCostSnapshot(float lastChargeCost,
                                               float dailyCost,
                                               float monthlyCost,
                                               float quarterlyCost) {
  portENTER_CRITICAL(&EmergencyCounterMux);
  gEmergencyLastChargeCost = lastChargeCost;
  gEmergencyDailyCost = dailyCost;
  gEmergencyMonthlyCost = monthlyCost;
  gEmergencyQuarterlyCost = quarterlyCost;
  portEXIT_CRITICAL(&EmergencyCounterMux);
}

static inline void updateLatestCostSnapshot(float lastChargeCost,
                                            float dailyCost,
                                            float monthlyCost,
                                            float quarterlyCost) {
  portENTER_CRITICAL(&CostMux);
  LatestLastChargeCost = lastChargeCost;
  LatestDailyCost = dailyCost;
  LatestMonthlyCost = monthlyCost;
  LatestQuarterlyCost = quarterlyCost;
  portEXIT_CRITICAL(&CostMux);
  updateEmergencyCostSnapshot(lastChargeCost, dailyCost, monthlyCost, quarterlyCost);
}

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

void requestLastChargeCostReset() {
  portENTER_CRITICAL(&CostResetMux);
  LastChargeCostResetPending = true;
  portEXIT_CRITICAL(&CostResetMux);
}

void requestDailyCostReset() {
  portENTER_CRITICAL(&CostResetMux);
  DailyCostResetPending = true;
  portEXIT_CRITICAL(&CostResetMux);
}

void requestMonthlyCostReset() {
  portENTER_CRITICAL(&CostResetMux);
  MonthlyCostResetPending = true;
  portEXIT_CRITICAL(&CostResetMux);
}

void requestQuarterlyCostReset() {
  portENTER_CRITICAL(&CostResetMux);
  QuarterlyCostResetPending = true;
  portEXIT_CRITICAL(&CostResetMux);
}

bool getLatestEnergyKwh(float* energyKwh) {
  if (!energyKwh) {
    return false;
  }

  // Energy is derived from the ISR-updated emergency counters, which are always current,
  // rather than LatestEnergyKwh (only updated by PulseInputTask after draining its queue).
  portENTER_CRITICAL(&EmergencyCounterMux);
  *energyKwh = (sPulsePerKwh > 0)
                 ? (float)gEmergencyPulseCounter / (float)sPulsePerKwh
                 : 0.0f;
  portEXIT_CRITICAL(&EmergencyCounterMux);
  return true;
}

bool getLatestEnergySnapshot(float* powerW, float* energyKwh, float* subtotalKwh) {
  if (!powerW || !energyKwh || !subtotalKwh) {
    return false;
  }

  portENTER_CRITICAL(&EmergencyCounterMux);
  *energyKwh = (sPulsePerKwh > 0)
                 ? (float)gEmergencyPulseCounter / (float)sPulsePerKwh
                 : 0.0f;
  *subtotalKwh = (sPulsePerKwh > 0)
                   ? (float)gEmergencySubtotalPulseCounter / (float)sPulsePerKwh
                   : 0.0f;
  portEXIT_CRITICAL(&EmergencyCounterMux);

  portENTER_CRITICAL(&EnergyKwhMux);
  *powerW = LatestPowerW;
  portEXIT_CRITICAL(&EnergyKwhMux);
  return true;
}

bool getLatestCostSnapshot(float* lastChargeCost,
                           float* dailyCost,
                           float* monthlyCost,
                           float* quarterlyCost) {
  if (!lastChargeCost || !dailyCost || !monthlyCost || !quarterlyCost) {
    return false;
  }

  portENTER_CRITICAL(&CostMux);
  *lastChargeCost = LatestLastChargeCost;
  *dailyCost = LatestDailyCost;
  *monthlyCost = LatestMonthlyCost;
  *quarterlyCost = LatestQuarterlyCost;
  portEXIT_CRITICAL(&CostMux);
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

static void loadCostFromNVS(float* lastChargeCost,
                            float* dailyCost,
                            float* monthlyCost,
                            float* quarterlyCost) {
  if (!lastChargeCost || !dailyCost || !monthlyCost || !quarterlyCost) {
    return;
  }

  Preferences pref;
  pref.begin(COUNT_NVS_NAMESPACE, true); // true = read-only
  *lastChargeCost = pref.getFloat("last_charge_cost", 0.0f);
  *dailyCost = pref.getFloat("daily_cost", 0.0f);
  *monthlyCost = pref.getFloat("monthly_cost", 0.0f);
  *quarterlyCost = pref.getFloat("quarterly_cost", 0.0f);
  pref.end();
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

static void saveCostToNVS(float lastChargeCost,
                          float dailyCost,
                          float monthlyCost,
                          float quarterlyCost) {
  Preferences pref;
  pref.begin(COUNT_NVS_NAMESPACE, false); // false = read/write
  pref.putFloat("last_charge_cost", lastChargeCost);
  pref.putFloat("daily_cost", dailyCost);
  pref.putFloat("monthly_cost", monthlyCost);
  pref.putFloat("quarterly_cost", quarterlyCost);
  pref.end();
}

static void saveControlledPowerCycleToNVS(bool controlledPowerCycle) {
  Preferences pref;
  pref.begin(COUNT_NVS_NAMESPACE, false); // false = read/write
  pref.putBool("controlled_pwr", controlledPowerCycle);
  pref.end();
}

void markControlledPowerCycleForNextBoot() {
  saveControlledPowerCycleToNVS(true);
}

// Persists the boot-cause classification for the next boot (NVS key "reset_cause").
// yieldToHard=true refuses to overwrite an already-recorded BOOT_CAUSE_HARD: a requested
// hard reset drives the same power cut that the direct-reset task reacts to, and the
// initiator of the reset must win the boot-reason label over the power-fail observer.
static void markBootResetCause(uint8_t cause, bool yieldToHard) {
  Preferences pref;
  pref.begin(COUNT_NVS_NAMESPACE, false); // false = read/write
  if (!yieldToHard || pref.getUChar("reset_cause", BOOT_CAUSE_NONE) != BOOT_CAUSE_HARD) {
    pref.putUChar("reset_cause", cause);
  }
  pref.end();
}

void markBootResetCauseForNextBoot(uint8_t cause) {
  markBootResetCause(cause, false);
}

static bool trySaveToNVS(uint32_t pulseCounter,
                         uint16_t subtotalPulseCounter,
                         float lastChargeCost,
                         float dailyCost,
                         float monthlyCost,
                         float quarterlyCost,
                         uint32_t& lastSavedPulseCounter,
                         uint16_t& lastSavedSubtotalPulseCounter,
                         float& lastSavedLastChargeCost,
                         float& lastSavedDailyCost,
                         float& lastSavedMonthlyCost,
                         float& lastSavedQuarterlyCost,
                         uint32_t& lastSaveMs,
                         bool& saveDeferredDuringOta) {
  if (isOtaInProgress()) {
    if (!saveDeferredDuringOta) {
      OledEnergyDisplay::showMonitorLine("NVS save deferred");
      saveDeferredDuringOta = true;
    }
    return false;
  }

  saveToNVS(pulseCounter, subtotalPulseCounter);
  saveCostToNVS(lastChargeCost, dailyCost, monthlyCost, quarterlyCost);
  lastSavedPulseCounter = pulseCounter;
  lastSavedSubtotalPulseCounter = subtotalPulseCounter;
  lastSavedLastChargeCost = lastChargeCost;
  lastSavedDailyCost = dailyCost;
  lastSavedMonthlyCost = monthlyCost;
  lastSavedQuarterlyCost = quarterlyCost;
  lastSaveMs = millis();
  saveDeferredDuringOta = false;
  return true;
}

/* ###################################################################################################
 *               R E S E T   F U N C T I O N A L I T Y
 * ###################################################################################################
 */
void requestReset(ResetType_t type) {
  portENTER_CRITICAL(&ResetMux);
  gResetType = type;
  gResetRequested = true;
  portEXIT_CRITICAL(&ResetMux);
}

#if ENABLE_DIRECT_RESET
static void directResetTask(void* pvParameters) {
  (void)pvParameters;
  while (true) {
    xSemaphoreTake(sDirectResetSemaphore, portMAX_DELAY);
    DirectResetTriggerCount++;
    // Skip NVS write during OTA: OTA is actively writing to flash on the same
    // SPI bus. A concurrent NVS (Preferences) write at max priority would
    // stall the OTA TCP receive task and cause upload timeouts.  The device
    // is about to be reflashed anyway, so the emergency save is unnecessary.
    if (isOtaInProgress()) {
      continue;
    }
    DirectResetActive = true;
    portENTER_CRITICAL(&EmergencyCounterMux);
    uint32_t pc = gEmergencyPulseCounter;
    uint16_t sc = gEmergencySubtotalPulseCounter;
    float lc = gEmergencyLastChargeCost;
    float dc = gEmergencyDailyCost;
    float mc = gEmergencyMonthlyCost;
    float qc = gEmergencyQuarterlyCost;
    portEXIT_CRITICAL(&EmergencyCounterMux);
    saveToNVS(pc, sc);
    saveCostToNVS(lc, dc, mc, qc);
    saveControlledPowerCycleToNVS(true);
    markBootResetCause(BOOT_CAUSE_DIRECT, true); // Yield to a requested hard reset: it initiated this power cut
    // MQTT notification is best effort and must never delay emergency NVS persistence.
    requestMqttOfflineStatus();
    DirectResetActive = false;

    #ifdef STACK_WATERMARK
    gDirectResetTaskStackHighWater = uxTaskGetStackHighWaterMark(nullptr);
    #endif
  }
}

void IRAM_ATTR DirectResetISR() {
  BaseType_t higherPriorityTaskWoken = pdFALSE;
  xSemaphoreGiveFromISR(sDirectResetSemaphore, &higherPriorityTaskWoken);
  portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

void startDirectResetISR(int gpio) {
  if (gpio < 0) {
    return;
  }
  sDirectResetSemaphore = xSemaphoreCreateBinary();
  if (!sDirectResetSemaphore) {
    return;
  }
  xTaskCreate(directResetTask, "direct_rst", DIRECT_RESET_TASK_STACK_SIZE, nullptr, configMAX_PRIORITIES - 1, nullptr);
  // Open-collector input requires pull-up bias to keep idle level stable.
  pinMode(gpio, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(gpio), DirectResetISR, RISING);
}

void suspendDirectResetISR() {
  if (DIRECT_RESET_GPIO >= 0) {
    detachInterrupt(digitalPinToInterrupt(DIRECT_RESET_GPIO));
  }
}

void resumeDirectResetISR() {
  if (DIRECT_RESET_GPIO >= 0) {
    attachInterrupt(digitalPinToInterrupt(DIRECT_RESET_GPIO), DirectResetISR, RISING);
  }
}
#else
// Direct-reset hardware confirmed defective; ISR/task fully disabled (see ENABLE_DIRECT_RESET above).
void startDirectResetISR(int) {}
void suspendDirectResetISR() {}
void resumeDirectResetISR() {}
#endif // ENABLE_DIRECT_RESET

void savePulseInputStateToNVS() {
  // Give PulseInputTask a brief window to drain any already-queued pulses so the
  // emergency counters reflect the latest processed count before this save.
  if (PulseInputQueue != nullptr) {
    uint32_t waitedMs = 0;
    while (uxQueueMessagesWaiting(PulseInputQueue) > 0 && waitedMs < 200) {
      vTaskDelay(pdMS_TO_TICKS(10));
      waitedMs += 10;
    }
  }

  portENTER_CRITICAL(&EmergencyCounterMux);
  uint32_t pc = gEmergencyPulseCounter;
  uint16_t sc = gEmergencySubtotalPulseCounter;
  float lc = gEmergencyLastChargeCost;
  float dc = gEmergencyDailyCost;
  float mc = gEmergencyMonthlyCost;
  float qc = gEmergencyQuarterlyCost;
  portEXIT_CRITICAL(&EmergencyCounterMux);
  saveToNVS(pc, sc);
  saveCostToNVS(lc, dc, mc, qc);
}

void initResetGpioPins() {
  // Initialize HARD_RESET_GPIO as early as possible in boot to prevent spurious power-cycle triggers.
  // Set output value LOW before switching mode to OUTPUT to avoid glitches from floating GPIO state.
  if (HARD_RESET_GPIO >= 0) {
    digitalWrite(HARD_RESET_GPIO, LOW);  // Preload output register with LOW
    pinMode(HARD_RESET_GPIO, OUTPUT);    // Then switch to OUTPUT mode (no glitch)
  }
}

/* ###################################################################################################
 *               P O W E R    C A L C U L A T I O N
 * ###################################################################################################
 */
float calculatePower( TaskParams_t* params,uint32_t deltaUs) {
  // Power calculation logic here
  float powerW = 0.0f;
  if (deltaUs > 0) {
    //  powerW = (3600.0f * 1000000.0f) / deltaUs; // Example calculation
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
  portENTER_CRITICAL_ISR(&PulseDiagnosticsMux);
  PulseIsrEdges++;
  if (xQueueSendFromISR(PulseInputQueue, &ts, &higherPriorityTaskWoken) == pdTRUE) {
    PulseQueuedEvents++;
  } else {
    PulseDroppedEvents++;
  }
  portEXIT_CRITICAL_ISR(&PulseDiagnosticsMux);
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

void getPulseInputDiagnostics(PulseInputDiagnostics_t* diagnostics) {
  if (!diagnostics) {
    return;
  }

  portENTER_CRITICAL(&PulseDiagnosticsMux);
  diagnostics->isrEdges = PulseIsrEdges;
  diagnostics->queuedEvents = PulseQueuedEvents;
  diagnostics->droppedEvents = PulseDroppedEvents;
  diagnostics->processedEvents = PulseProcessedEvents;
  diagnostics->taskHeartbeats = PulseTaskHeartbeats;
  portEXIT_CRITICAL(&PulseDiagnosticsMux);
  diagnostics->taskStage = PulseTaskStage;
  diagnostics->directResetTriggers = DirectResetTriggerCount;
  diagnostics->directResetActive = DirectResetActive;
}

static int sPulseInputGpio = -1;
static int sPulseInputMode = -1;

bool attachPulseInputInterrupt(int gpio, int mode, int pinInputMode) {
  if (gpio < 0 || PulseInputQueue == nullptr) {
    return false;
  }
  sPulseInputGpio = gpio;
  sPulseInputMode = mode;
  pinMode(gpio, pinInputMode);
  attachInterrupt(digitalPinToInterrupt(gpio), PulseInputISR, mode);
  return true;
}

void suspendPulseInputISR() {
  if (sPulseInputGpio >= 0) {
    detachInterrupt(digitalPinToInterrupt(sPulseInputGpio));
  }
}

void resumePulseInputISR() {
  if (sPulseInputGpio >= 0 && sPulseInputMode >= 0) {
    attachInterrupt(digitalPinToInterrupt(sPulseInputGpio), PulseInputISR, sPulseInputMode);
  }
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
  float lastChargeCost = 0.0f;
  float dailyCost = 0.0f;
  float monthlyCost = 0.0f;
  float quarterlyCost = 0.0f;
  loadCostFromNVS(&lastChargeCost, &dailyCost, &monthlyCost, &quarterlyCost);
  updateEmergencyCounters(pulseCounter, subtotalPulseCounter);
  float powerW = 0.0f;
  float energyKwh = (float)pulseCounter / (float)((TaskParams_t*)pvParameters)->pulse_per_kWh;
  float subtotalKwh = (float)subtotalPulseCounter / (float)((TaskParams_t*)pvParameters)->pulse_per_kWh;

  updateLatestEnergySnapshot(powerW, energyKwh, subtotalKwh);
  updateLatestCostSnapshot(lastChargeCost, dailyCost, monthlyCost, quarterlyCost);

  uint32_t lastSaveMs = millis();
  uint32_t lastSavedPulseCounter = pulseCounter;
  uint16_t lastSavedSubtotalPulseCounter = subtotalPulseCounter;
  float lastSavedLastChargeCost = lastChargeCost;
  float lastSavedDailyCost = dailyCost;
  float lastSavedMonthlyCost = monthlyCost;
  float lastSavedQuarterlyCost = quarterlyCost;
  bool saveDeferredDuringOta = false;

                                                    #ifdef HEADLESS_DEBUG
                                                      OledEnergyDisplay::showMonitorLine("Pulse task init");
                                                    #endif

                                                    #ifdef DEBUG
                                                    Serial.println("Pulse Input Task initializing...\n");
                                                    #endif

  if (PulseInputQueue == nullptr) {

                                                    #ifdef HEADLESS_DEBUG
                                                      OledEnergyDisplay::showMonitorLine("Pulse q not init");
                                                    #endif

                                                    #ifdef DEBUG
                                                    Serial.println("Pulse count queue not initialized!");
                                                    #endif
    
    PulseInputTaskHandle = nullptr;
    vTaskDelete(nullptr);
    return;
  }

  PulseInputTaskReady = true;

                                                    #ifdef HEADLESS_DEBUG
                                                      OledEnergyDisplay::showMonitorLine("Pulse task ready");
                                                      OledEnergyDisplay::showMonitorLine("Pulse task start");
                                                      OledEnergyDisplay::showMonitorLine("NVS count: " + String(pulseCounter));
                                                      OledEnergyDisplay::showMonitorLine("PT corr: " + String(((TaskParams_t*)pvParameters)->ptCorrection));
                                                      OledEnergyDisplay::showMonitorLine("Pulses/kWh: " + String(((TaskParams_t*)pvParameters)->pulse_per_kWh));
                                                      OledEnergyDisplay::showMonitorLine("Wait pulses");
                                                    #endif
 
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
    portENTER_CRITICAL(&PulseDiagnosticsMux);
    PulseTaskHeartbeats++;
    portEXIT_CRITICAL(&PulseDiagnosticsMux);
    setPulseTaskStage(STAGE_LOOP_TOP);

    // ---- Reset check ----
    setPulseTaskStage(STAGE_RESET_CHECK);
    bool shouldReset = false;
    ResetType_t resetType = RESET_SOFT;
    portENTER_CRITICAL(&ResetMux);
    if (gResetRequested) {
      shouldReset = true;
      resetType = gResetType;
    }
    portEXIT_CRITICAL(&ResetMux);

    if (shouldReset) {
      saveToNVS(pulseCounter, subtotalPulseCounter);
      saveCostToNVS(lastChargeCost, dailyCost, monthlyCost, quarterlyCost);
      // Any requested reset is intentional: mark the next boot as controlled so the
      // uncontrolled-boot safety net does not fire again after it (also prevents a
      // reset loop when the hard-reset hardware fails and the fallback below fires).
      saveControlledPowerCycleToNVS(true);
      if (resetType == RESET_HARD) {
        // Record the cause before power-cycling so the next boot reports HARD_RESET even
        // though a real power cycle comes back as ESP_RST_POWERON.
        markBootResetCause(BOOT_CAUSE_HARD, false);
        if (HARD_RESET_GPIO >= 0) {
          digitalWrite(HARD_RESET_GPIO, HIGH); // Trigger external power-cycle hardware
          // Grace period: give the external circuit time to physically reset the board.
          // If this task is still running when it expires (circuit not wired, faulty
          // transistor, ...), fall back to esp_restart() so a RESET_HARD request can
          // never park here forever. Pulses arriving during the grace period stay queued
          // unprocessed and are lost either way (by the power cut or by the fallback).
          vTaskDelay(pdMS_TO_TICKS(HARD_RESET_FALLBACK_TIMEOUT_MS));
          digitalWrite(HARD_RESET_GPIO, LOW); // Still alive: hardware did not respond.
        }
      } else {
        markBootResetCause(BOOT_CAUSE_SOFT, false); // RESET_SOFT was requested (e.g. MQTT command)
      }
      esp_restart();
      while (true) { vTaskDelay(portMAX_DELAY); } // Should not reach here
    }

    setPulseTaskStage(STAGE_PENDING_COUNTER);
    if (PulseCounterUpdatePending) {
      portENTER_CRITICAL(&PulseCounterMux);
      uint32_t previousPulseCounter = pulseCounter;
      pulseCounter = PendingPulseCounter;
      PulseCounterUpdatePending = false;
      portEXIT_CRITICAL(&PulseCounterMux);

      updateEmergencyCounters(pulseCounter, subtotalPulseCounter);

      float energyKwh = (float)pulseCounter / (float)((TaskParams_t*)pvParameters)->pulse_per_kWh;
      float subtotalKwh = (float)subtotalPulseCounter / (float)((TaskParams_t*)pvParameters)->pulse_per_kWh;

      updateLatestEnergySnapshot(powerW, energyKwh, subtotalKwh);
      publishMqttEnergy(0.0f, energyKwh, subtotalKwh);

      if (pulseCounter != previousPulseCounter) {
        trySaveToNVS(pulseCounter,
                     subtotalPulseCounter,
                     lastChargeCost,
                     dailyCost,
                     monthlyCost,
                     quarterlyCost,
                     lastSavedPulseCounter,
                     lastSavedSubtotalPulseCounter,
                     lastSavedLastChargeCost,
                     lastSavedDailyCost,
                     lastSavedMonthlyCost,
                     lastSavedQuarterlyCost,
                     lastSaveMs,
                     saveDeferredDuringOta);
      }
    }

    setPulseTaskStage(STAGE_SUBTOTAL_RESET);
    bool shouldResetSubtotal = false;
    portENTER_CRITICAL(&SubtotalResetMux);
    if (SubtotalResetPending) {
      SubtotalResetPending = false;
      shouldResetSubtotal = true;
    }
    portEXIT_CRITICAL(&SubtotalResetMux);

    if (shouldResetSubtotal) {
      // Avoid stack-heavy MQTT log formatting in this task.
      // PulseInputTask has a tighter stack budget, and this path is time-critical.

      bool subtotalChanged = subtotalPulseCounter != 0;
      subtotalPulseCounter = 0;
      updateEmergencyCounters(pulseCounter, subtotalPulseCounter);
      if (subtotalChanged) {
        trySaveToNVS(pulseCounter,
                     subtotalPulseCounter,
                     lastChargeCost,
                     dailyCost,
                     monthlyCost,
                     quarterlyCost,
                     lastSavedPulseCounter,
                     lastSavedSubtotalPulseCounter,
                     lastSavedLastChargeCost,
                     lastSavedDailyCost,
                     lastSavedMonthlyCost,
                     lastSavedQuarterlyCost,
                     lastSaveMs,
                     saveDeferredDuringOta);
      }

      float energyKwh = (float)pulseCounter / (float)((TaskParams_t*)pvParameters)->pulse_per_kWh;
      float subtotalKwh = 0.0f;
      updateLatestEnergySnapshot(powerW, energyKwh, subtotalKwh);
      publishMqttEnergy(0.0f, energyKwh, subtotalKwh);
    }

    setPulseTaskStage(STAGE_COST_RESET);
    bool resetLastChargeCost = false;
    bool resetDailyCost = false;
    bool resetMonthlyCost = false;
    bool resetQuarterlyCost = false;
    portENTER_CRITICAL(&CostResetMux);
    if (LastChargeCostResetPending) {
      LastChargeCostResetPending = false;
      resetLastChargeCost = true;
    }
    if (DailyCostResetPending) {
      DailyCostResetPending = false;
      resetDailyCost = true;
    }
    if (MonthlyCostResetPending) {
      MonthlyCostResetPending = false;
      resetMonthlyCost = true;
    }
    if (QuarterlyCostResetPending) {
      QuarterlyCostResetPending = false;
      resetQuarterlyCost = true;
    }
    portEXIT_CRITICAL(&CostResetMux);

    if (resetLastChargeCost || resetDailyCost || resetMonthlyCost || resetQuarterlyCost) {
      if (resetLastChargeCost) {
        lastChargeCost = 0.0f;
      }
      if (resetDailyCost) {
        dailyCost = 0.0f;
      }
      if (resetMonthlyCost) {
        monthlyCost = 0.0f;
      }
      if (resetQuarterlyCost) {
        quarterlyCost = 0.0f;
      }

      updateLatestCostSnapshot(lastChargeCost, dailyCost, monthlyCost, quarterlyCost);

      trySaveToNVS(pulseCounter,
                   subtotalPulseCounter,
                   lastChargeCost,
                   dailyCost,
                   monthlyCost,
                   quarterlyCost,
                   lastSavedPulseCounter,
                   lastSavedSubtotalPulseCounter,
                   lastSavedLastChargeCost,
                   lastSavedDailyCost,
                   lastSavedMonthlyCost,
                   lastSavedQuarterlyCost,
                   lastSaveMs,
                   saveDeferredDuringOta);

      publishMqttEnergy(powerW, energyKwh, subtotalKwh);
    }

    // Wait for pulse timestamp from ISR
    setPulseTaskStage(STAGE_QUEUE_WAIT);
    if (xQueueReceive(PulseInputQueue, &ts, pdMS_TO_TICKS(1000))) {

      portENTER_CRITICAL(&PulseDiagnosticsMux);
      PulseProcessedEvents++;
      portEXIT_CRITICAL(&PulseDiagnosticsMux);
      setPulseTaskStage(STAGE_PULSE_PROCESSED);

      sendLedCommand(LedId::Charge, "Blink");

                                          #ifdef HEADLESS_DEBUG
                                            OledEnergyDisplay::showMonitorLine("Pulse ts: " + String(ts));
                                          #endif
      
                                          #ifdef DEBUG
                                          Serial.println("\nProcessing pulse count and timestamp: " + String(ts) + "\n");
                                          #endif

      // ---- 1. Pulse counting ----
      pulseCounter++;
      subtotalPulseCounter++;
      float pulseCost = 0.0f;
      if (((TaskParams_t*)pvParameters)->pulse_per_kWh > 0) {
        pulseCost = gCurrentEnergyPrice / (float)((TaskParams_t*)pvParameters)->pulse_per_kWh;
      }
      if (isChargingSessionCharging()) {
        lastChargeCost += pulseCost;
      }
      dailyCost += pulseCost;
      monthlyCost += pulseCost;
      quarterlyCost += pulseCost;
      updateLatestCostSnapshot(lastChargeCost, dailyCost, monthlyCost, quarterlyCost);
      updateEmergencyCounters(pulseCounter, subtotalPulseCounter);

                                          #ifdef HEADLESS_DEBUG
                                            OledEnergyDisplay::showMonitorLine("Cnt: " + String(pulseCounter) + " Sub:" + String(subtotalPulseCounter));
                                          #endif

                                          #ifdef DEBUG
                                          Serial.println("\nPulse Count: " + String(pulseCounter) + " Subtotal Pulse Count: " + String(subtotalPulseCounter));
                                          #endif

      // ---- 2. Power calculation ----
      if (lastTs > 0) {
          uint32_t deltaUs = ts - lastTs;
          powerW = calculatePower( (TaskParams_t*)pvParameters, deltaUs);

                                                          #ifdef HEADLESS_DEBUG
                                                            OledEnergyDisplay::showMonitorLine("Pwr: " + String(powerW, 0) + "W");
                                                          #endif

                                                          #ifdef DEBUG
                                                            Serial.println("\nDelta U sec: " + String(deltaUs) + " Pulse Count: " + String(pulseCounter) + " Power: " + String(powerW) + " W");
                                                          #endif

      }
      lastTs = ts;

      float energyKwh = (float)pulseCounter / (float)((TaskParams_t*)pvParameters)->pulse_per_kWh;
      float subtotalKwh = (float)subtotalPulseCounter / (float)((TaskParams_t*)pvParameters)->pulse_per_kWh;
      
      updateLatestEnergySnapshot(powerW, energyKwh, subtotalKwh);
      publishMqttEnergy(powerW, energyKwh, subtotalKwh);
    }

    // ---- 3. Power calculation even if no new pulse (to update power to 0 if pulses stop) ----
    setPulseTaskStage(STAGE_POWER_DECAY_CHECK);
    if (lastTs > 0 && powerW > 0.5f && micros() > lastTs) { // If micros < lastTs, micros has overrrun. In that case we keep the last power until next pulse to avoid incorrect 0 reading.
        uint32_t deltaUs = micros() - lastTs; // Time since last pulse in microseconds
        float possiblePowerW = calculatePower( (TaskParams_t*)pvParameters, deltaUs);

        // If power has dropped significantly (e.g. more than 50%), update it to reflect possible stop of consumption
        if ((2 * possiblePowerW) < powerW) { 
          powerW = possiblePowerW;

          updateLatestEnergySnapshot(powerW, energyKwh, subtotalKwh);
          publishMqttEnergy(powerW, energyKwh, subtotalKwh);

                                                          #ifdef HEADLESS_DEBUG
                                                            OledEnergyDisplay::showMonitorLine("Pwr upd: " + String(powerW, 0) + "W");
                                                          #endif

                                                          #ifdef DEBUG
                                                            Serial.println("\nNo new pulse but updating power: " + String(powerW) + " W");
                                                          #endif
        }
    }

                                                          #ifdef DEBUG
                                                            Serial.print (".");
                                                          #endif  

    // ---- 4. Periodic NVS save ----
    setPulseTaskStage(STAGE_PERIODIC_SAVE_START);
    if (millis() - lastSaveMs >= SAVE_INTERVAL_MS) {
      bool hasCounterChanges = (pulseCounter != lastSavedPulseCounter) ||
                               (subtotalPulseCounter != lastSavedSubtotalPulseCounter);

      if (hasCounterChanges) {

                                          #ifdef DEBUG
                                          Serial.println("\nSaving pulse count to NVS: " + String(pulseCounter) + "\n");
                                          #endif

        trySaveToNVS(pulseCounter,
                     subtotalPulseCounter,
                     lastChargeCost,
                     dailyCost,
                     monthlyCost,
                     quarterlyCost,
                     lastSavedPulseCounter,
                     lastSavedSubtotalPulseCounter,
                     lastSavedLastChargeCost,
                     lastSavedDailyCost,
                     lastSavedMonthlyCost,
                     lastSavedQuarterlyCost,
                     lastSaveMs,
                     saveDeferredDuringOta);
      }
    }
    setPulseTaskStage(STAGE_PERIODIC_SAVE_DONE);

                                                            #ifdef STACK_WATERMARK
                                                            static uint32_t lastLog = 0;
                                                            if (millis() - lastLog > 5000) {
                                                              lastLog = millis();
                                                              gPulseInputTaskStackHighWater = uxTaskGetStackHighWaterMark(nullptr);
                                                            }
                                                            #endif

    setPulseTaskStage(STAGE_LOOP_END);
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

  if (params != nullptr) {
    sPulsePerKwh = params->pulse_per_kWh;
  }

  // HARD_RESET_GPIO is already initialized safely in initResetGpioPins() during boot.
  // Verify it is still in safe state during task startup.
  if (HARD_RESET_GPIO >= 0) {
    digitalWrite(HARD_RESET_GPIO, LOW); // Ensure reset line remains inactive
  }

  // Seed emergency counters before enabling the direct-reset ISR so an
  // early direct-reset event persists the latest stored values, not zeros.
  uint16_t bootSubtotalPulseCounter = 0;
  uint32_t bootPulseCounter = loadFromNVS(&bootSubtotalPulseCounter);
  updateEmergencyCounters(bootPulseCounter, bootSubtotalPulseCounter);

  startDirectResetISR(DIRECT_RESET_GPIO);

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

