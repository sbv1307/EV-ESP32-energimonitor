#pragma once

#include <Arduino.h>

#ifndef OLED_ENERGY_DISPLAY_DEFAULT_I2C_SDA
#define OLED_ENERGY_DISPLAY_DEFAULT_I2C_SDA 21
#endif

#ifndef OLED_ENERGY_DISPLAY_DEFAULT_I2C_SCL
#define OLED_ENERGY_DISPLAY_DEFAULT_I2C_SCL 22
#endif

#ifndef OLED_ENERGY_DISPLAY_DEFAULT_I2C_ADDRESS
#define OLED_ENERGY_DISPLAY_DEFAULT_I2C_ADDRESS 0x3C
#endif

#ifndef OLED_ENERGY_DISPLAY_DEFAULT_CHARGING_ICON_BLINK_MS
#define OLED_ENERGY_DISPLAY_DEFAULT_CHARGING_ICON_BLINK_MS 400
#endif

namespace OledEnergyDisplay {
struct Settings {
	uint8_t i2cSda = OLED_ENERGY_DISPLAY_DEFAULT_I2C_SDA;
	uint8_t i2cScl = OLED_ENERGY_DISPLAY_DEFAULT_I2C_SCL;
	uint8_t i2cAddress = OLED_ENERGY_DISPLAY_DEFAULT_I2C_ADDRESS;
	uint32_t chargingIconBlinkIntervalMs =
			OLED_ENERGY_DISPLAY_DEFAULT_CHARGING_ICON_BLINK_MS;
};

bool begin();
bool begin(const Settings& settings);
void showSplash(const String& projectNameVersion, uint32_t durationMs = 5000);
void showEnergy(float energyKwh, bool charging = false, float chargeEnergyKwh = 0.0f,
				bool smartChargingActivated = false, const char* timeBuf = nullptr,
				float currentEnergyPrice = 0.0f,
				float energyPriceLlimit = 0.0f);
void update();
void turnOff();
void turnOn();
bool isOn();
}
