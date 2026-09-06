#ifndef TASK_DIMMER
#define TASK_DIMMER

#include <Arduino.h>
#include "../config/config.h"
#include "../config/enums.h"
#include "../functions/dimmerFunction.h"

extern DisplayValues gDisplayValues;

/**
 * Task: update the autonomous dimmer regulation.
 *
 * Fronius measurements are refreshed independently every 1-2 seconds. This
 * task runs more frequently so a newly detected Fronius failure can force
 * POWER=0 quickly, while dimmerFunction only sends HTTP when the requested
 * power changes by >=1% or the 60-second keepalive is due.
 */
void updateDimmer(void * parameter) {
  (void)parameter;

  for (;;) {
    gDisplayValues.task = true;

#if WIFI_ACTIVE == true
    dimmer();
#endif

    gDisplayValues.task = false;
    vTaskDelay(250 / portTICK_PERIOD_MS);
  }
}

#endif
