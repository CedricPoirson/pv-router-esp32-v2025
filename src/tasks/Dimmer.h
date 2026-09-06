#ifndef TASK_DIMMER
#define TASK_DIMMER

#include <Arduino.h>
#include "../config/config.h"
#include "../config/enums.h"
#include "../functions/dimmerFunction.h"
#include "../functions/froniusZeroGrid.h"

extern DisplayValues gDisplayValues;

/**
 * Task: regulate the dimmer from the active electricity measurement source.
 *
 * When Fronius is available, V12 is the only controller allowed to change
 * the dimmer command. The legacy watt-based controller is used only when
 * Fronius is unavailable, avoiding two regulators fighting each other.
 */
void updateDimmer(void * parameter){
  for (;;){
    gDisplayValues.task = true;

#if WIFI_ACTIVE == true
    if (gDisplayValues.froniusup == true) {
      froniusZeroGridSimulation();
    }
    else {
      dimmer();
    }
#endif

    gDisplayValues.task = false;

    // Keep the control loop slower than the Fronius acquisition loop so each
    // decision normally uses a fresh grid measurement.
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

#endif
