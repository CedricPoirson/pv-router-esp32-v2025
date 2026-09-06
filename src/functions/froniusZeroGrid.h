#ifndef FRONIUS_ZERO_GRID_H
#define FRONIUS_ZERO_GRID_H

// Experimental Fronius Zero Grid controller
// V7 - power based regulation for scalable heater power

#include "config/enums.h"

// Positive P_Grid = import from grid
// Negative P_Grid = export to grid

#define FRONIUS_GRID_MARGIN_W 20
#define FRONIUS_HEATER_POWER_W 800
#define FRONIUS_MAX_DIMMER 100
#define FRONIUS_STEP_FAST 10
#define FRONIUS_STEP_SLOW 3

extern DisplayValues gDisplayValues;

static int simulatedDimmer = 0;

void froniusZeroGridSimulation()
{
    int grid = (int)gDisplayValues.grid;
    int production = (int)gDisplayValues.production;
    int consumedPower = 0;
    int remainingExport = 0;
    int targetDimmer = simulatedDimmer;
    int correction = 0;

    if (grid < -FRONIUS_GRID_MARGIN_W) {
        int surplus = abs(grid);

        targetDimmer = (surplus * 100) / FRONIUS_HEATER_POWER_W;
        if (targetDimmer > FRONIUS_MAX_DIMMER)
            targetDimmer = FRONIUS_MAX_DIMMER;

        consumedPower = (FRONIUS_HEATER_POWER_W * simulatedDimmer) / 100;
        remainingExport = surplus - consumedPower;

        int delta = targetDimmer - simulatedDimmer;
        if (abs(delta) > 20)
            correction = (delta > 0) ? FRONIUS_STEP_FAST : -FRONIUS_STEP_FAST;
        else if (abs(delta) > 5)
            correction = (delta > 0) ? FRONIUS_STEP_SLOW : -FRONIUS_STEP_SLOW;
        else
            correction = delta;
    }
    else if (grid > FRONIUS_GRID_MARGIN_W) {
        // Protect against cloud and grid import
        correction = -FRONIUS_STEP_FAST;
    }

    simulatedDimmer += correction;

    if (simulatedDimmer < 0)
        simulatedDimmer = 0;
    if (simulatedDimmer > FRONIUS_MAX_DIMMER)
        simulatedDimmer = FRONIUS_MAX_DIMMER;

    gDisplayValues.dimmer = simulatedDimmer;

    Serial.println();
    Serial.println("========== FRONIUS ZERO GRID SIMU V7 ==========");
    Serial.printf("PV production : %d W\n", production);
    Serial.printf("Grid exchange : %d W\n", grid);
    Serial.printf("Heater max    : %d W\n", FRONIUS_HEATER_POWER_W);
    Serial.printf("Dimmer actual : %d %%\n", simulatedDimmer);
    Serial.printf("Target dimmer : %d %%\n", targetDimmer);
    Serial.printf("Heater power  : %d W\n", consumedPower);
    Serial.printf("Export left   : %d W\n", remainingExport);

    if (grid < -FRONIUS_GRID_MARGIN_W)
        Serial.println("Status        : SURPLUS PV");
    else if (grid > FRONIUS_GRID_MARGIN_W)
        Serial.println("Status        : IMPORT RESEAU");
    else
        Serial.println("Status        : ZERO GRID OK");

    Serial.printf("Correction    : %d %%\n", correction);
    Serial.println("Decision      : POWER BASED ADAPTIVE UPDATE");
    Serial.println("===============================================");
}

#endif
