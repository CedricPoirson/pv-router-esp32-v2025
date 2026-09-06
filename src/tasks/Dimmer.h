#ifndef TASK_DIMMER
#define TASK_DIMMER

#include <Arduino.h>
#include "../config/config.h"
#include "../config/enums.h"
#include "../functions/dimmerFunction.h"
#include "../functions/froniusZeroGrid.h"

extern DisplayValues gDisplayValues;
extern volatile uint32_t gFroniusSampleCounter;

/**
 * Task: regulate the dimmer from fresh Fronius PowerFlow samples.
 *
 * V12.1 no longer waits for an independent 5 s control timer. The task wakes
 * frequently but executes the Zero Grid controller exactly once for each new
 * validated Fronius sample. This keeps the official ~4 s Fronius polling
 * cadence while removing up to ~5 s of extra control latency.
 */
void updateDimmer(void * parameter){
  uint32_t lastProcessedFroniusSample = 0;

  for (;;){
    gDisplayValues.task = true;

#if WIFI_ACTIVE == true
    if (gDisplayValues.froniusup == true) {
      const uint32_t sample = gFroniusSampleCounter;

      if (sample != 0 && sample != lastProcessedFroniusSample) {
        froniusZeroGridSimulation();
        lastProcessedFroniusSample = sample;
      }
    }
    // When the Fronius sample is invalid we simply hold the last command for
    // now. Communication watchdog / fail-safe shutdown will be added later.
#endif

    gDisplayValues.task = false;

    // Fast local wake-up; no additional Fronius HTTP call is made here.
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

#endif
