#ifndef TASK_DIMMER
#define TASK_DIMMER

#include <Arduino.h>
#include "../config/config.h"
#include "../config/enums.h"
#include "../functions/dimmerFunction.h"
#include "../functions/froniusZeroGrid.h"

extern DisplayValues gDisplayValues;
extern Config config;
extern volatile uint32_t gFroniusSampleCounter;

/**
 * Regulate the dimmer exactly once for every fresh validated Fronius sample.
 * The task also runs a fast local watchdog so a failed/stale Fronius source,
 * a voluntary routing stop, or a remote dimmer safety condition immediately
 * requests POWER=0 without depending on MQTT or Home Assistant.
 */
void updateDimmer(void * parameter){
  (void)parameter;

  uint32_t lastProcessedFroniusSample = 0;

  for (;;) {
    gDisplayValues.task = true;

#if WIFI_ACTIVE == true
    const unsigned long now = millis();

    const bool froniusFresh =
        gDisplayValues.froniusup &&
        gDisplayValues.froniusLastOkMs > 0 &&
        ((unsigned long)(now - gDisplayValues.froniusLastOkMs) <= FRONIUS_STALE_MS);

    const bool dimmerStateFresh =
        gDisplayValues.dimmerCommOk &&
        gDisplayValues.dimmerLastOkMs > 0 &&
        ((unsigned long)(now - gDisplayValues.dimmerLastOkMs) <= DIMMER_STATE_FRESH_MS);

    const bool remoteSafetyStop =
        dimmerStateFresh &&
        (!gDisplayValues.dimmerOn || gDisplayValues.dimmerAlarm);

    if (!froniusFresh) {
      froniusZeroGridFailsafe("Fronius unavailable/stale");
    }
    else if (!config.autonome) {
      froniusZeroGridFailsafe("routing disabled");
    }
    else if (remoteSafetyStop) {
      froniusZeroGridFailsafe(
          gDisplayValues.dimmerAlarm ? "dimmer alarm" : "dimmer onoff=false");
    }
    else {
      const uint32_t sample = gFroniusSampleCounter;

      if (sample != 0 && sample != lastProcessedFroniusSample) {
        froniusZeroGridSimulation();
        lastProcessedFroniusSample = sample;
      }
    }
#endif

    gDisplayValues.task = false;

    // Fast watchdog wake-up; all network calls retain short timeouts.
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

#endif
