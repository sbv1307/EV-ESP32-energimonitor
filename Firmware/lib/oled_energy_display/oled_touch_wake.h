#pragma once

#include <stdint.h>

#ifndef OLED_TOUCH_WAKE_DEFAULT_INPUT_GPIO
#define OLED_TOUCH_WAKE_DEFAULT_INPUT_GPIO 4
#endif

#ifndef OLED_TOUCH_WAKE_DEFAULT_DISPLAY_ON_TIME_MS
#define OLED_TOUCH_WAKE_DEFAULT_DISPLAY_ON_TIME_MS 30000
#endif

#ifndef OLED_TOUCH_WAKE_DEFAULT_SAMPLE_INTERVAL_MS
#define OLED_TOUCH_WAKE_DEFAULT_SAMPLE_INTERVAL_MS 50
#endif

#ifndef OLED_TOUCH_WAKE_DEFAULT_MIN_DELTA
#define OLED_TOUCH_WAKE_DEFAULT_MIN_DELTA 12
#endif

// 4 samples = 200 ms at the default 50 ms interval: rejects the ~100-150 ms
// EMI bursts observed in the field while still passing a real finger touch.
#ifndef OLED_TOUCH_WAKE_DEFAULT_DEBOUNCE_COUNT
#define OLED_TOUCH_WAKE_DEFAULT_DEBOUNCE_COUNT 4
#endif

namespace OledTouchWake {
struct Settings {
	uint8_t inputGpio = OLED_TOUCH_WAKE_DEFAULT_INPUT_GPIO;
	uint32_t displayOnTimeMs = OLED_TOUCH_WAKE_DEFAULT_DISPLAY_ON_TIME_MS;
	uint32_t sampleIntervalMs = OLED_TOUCH_WAKE_DEFAULT_SAMPLE_INTERVAL_MS;
	uint16_t minDelta = OLED_TOUCH_WAKE_DEFAULT_MIN_DELTA;
	uint8_t debounceCount = OLED_TOUCH_WAKE_DEFAULT_DEBOUNCE_COUNT;
};

struct Diagnostics {
	uint16_t lastValue = 0;
	uint16_t baseline = 0;
	uint16_t threshold = 0;
	uint8_t consecutiveHits = 0;
	bool eventLatched = false;
	bool displayOn = false;
	uint32_t samples = 0;
	uint32_t thresholdHits = 0;
	uint32_t wakeEvents = 0;
};

void begin();
void begin(const Settings& settings);
void armDisplayOnTimer();
void update();
void getDiagnostics(Diagnostics* diagnostics);
}
