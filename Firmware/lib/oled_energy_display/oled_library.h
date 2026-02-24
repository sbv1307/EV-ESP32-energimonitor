#pragma once

#include "oled_energy_display.h"
#include "oled_touch_wake.h"

namespace OledLibrary {
struct Settings {
  OledEnergyDisplay::Settings energyDisplay;
  OledTouchWake::Settings touchWake;
  bool showSplashOnBoot = false;
  const char* splashText = nullptr;
  uint32_t splashDurationMs = 5000;
  bool turnOffAfterSplash = true;
};

bool begin();
bool begin(const Settings& settings);
void update();

bool startBackgroundUpdater(uint32_t intervalMs = 20,
                           uint32_t stackSizeWords = 1424,
                           uint32_t priority = 1,
                           int8_t coreId = 1);
void stopBackgroundUpdater();
bool isBackgroundUpdaterRunning();
}
