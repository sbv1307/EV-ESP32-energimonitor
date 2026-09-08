#pragma once
#include "globals.h"
#include <Arduino.h>

void startPulseInputTask(TaskParams_t* params);

void initResetGpioPins(); // Initialize HARD_RESET_GPIO early in boot (before task startup)

void IRAM_ATTR PulseInputISR();

bool isPulseInputReady();

bool waitForPulseInputReady(uint32_t timeoutMs);

// Immediately persists the latest pulse/cost counters to NVS, bypassing the periodic
// save interval. Call before any reboot not driven by PulseInputTask's own reset path
// (e.g. before an OTA-triggered restart) to avoid losing recently counted pulses.
void savePulseInputStateToNVS();

// Marks the next boot as a controlled power cycle (NVS "controlled_pwr" = true). Call before
// any intentional reboot that does not pass through PulseInputTask's own reset handling
// (e.g. the OTA-triggered restart) so the uncontrolled-boot safety net stays quiet after it.
void markControlledPowerCycleForNextBoot();

struct PulseInputDiagnostics_t {
  uint32_t isrEdges;
  uint32_t queuedEvents;
  uint32_t droppedEvents;
  uint32_t processedEvents;
  uint32_t taskHeartbeats;
  uint32_t taskStage;          // Last PulseInputTask loop stage reached (see PulseInputStage_t)
  uint32_t directResetTriggers; // Number of direct-reset ISR triggers since boot
  bool     directResetActive;   // true while directResetTask is mid NVS-write
};

void getPulseInputDiagnostics(PulseInputDiagnostics_t* diagnostics);

bool attachPulseInputInterrupt(int gpio, int mode, int pinInputMode = INPUT);

void suspendPulseInputISR(); // Detach pulse interrupt (call during OTA)
void resumePulseInputISR();  // Re-attach pulse interrupt (call after OTA)

void suspendDirectResetISR(); // Detach direct-reset interrupt (call during OTA)
void resumeDirectResetISR();  // Re-attach direct-reset interrupt (call after OTA)

void setPulseCounterFromMqtt(uint32_t newPulseCounter);

bool getLatestEnergyKwh(float* energyKwh);

bool getLatestEnergySnapshot(float* powerW, float* energyKwh, float* subtotalKwh);

bool getLatestCostSnapshot(float* lastChargeCost,
                           float* dailyCost,
                           float* monthlyCost,
                           float* quarterlyCost);

void requestSubtotalReset();
void requestLastChargeCostReset();
void requestDailyCostReset();
void requestMonthlyCostReset();
void requestQuarterlyCostReset();

typedef enum {
  RESET_SOFT,
  RESET_HARD,
} ResetType_t;

void requestReset(ResetType_t type);