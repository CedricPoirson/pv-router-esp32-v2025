#ifndef FRONIUS_ZERO_GRID_H
#define FRONIUS_ZERO_GRID_H

// Experimental Fronius Zero Grid controller
// V8 - faster adaptive regulation and cloud protection

#include "config/enums.h"

// Positive P_Grid = import from grid
// Negative P_Grid = export to grid

#define FRONIUS_GRID_MARGIN_W 20
#define FRONIUS_HEATER_POWER_W 800
#define FRONIUS_MAX_DIMMER 100
#define FRONIUS_STEP_FAST 15
#define FRONIUS_STEP_SLOW 5
#define FRONIUS_STEP_FINE 1

extern DisplayValues gDisplayValues;

static int simulatedDimmer = 0;
static int lastGrid = 0;

void froniusZeroGridSimulation()
{
    int grid = (int)gDisplayValues.grid;
    int production = (int)gDisplayValues.production;
    int surplus = 0;
    int heaterPower = 0;
    int exportLeft = 0;
    int targetDimmer = simulatedDimmer;
    int correction = 0;

    if (grid < -FRONIUS_GRID_MARGIN_W) {
        surplus = abs(grid);
        targetDimmer = (surplus * 100) / FRONIUS_HEATER_POWER_W;

        if (targetDimmer > FRONIUS_MAX_DIMMER)
            targetDimmer = FRONIUS_MAX_DIMMER;

        int delta = targetDimmer - simulatedDimmer;

        // Faster response when large surplus appears
        if (abs(delta) > 40)
            correction = (delta > 0) ? FRONIUS_STEP_FAST : -FRONIUS_STEP_FAST;
        else if (abs(delta) > 10)
            correction = (delta > 0) ? FRONIUS_STEP_SLOW : -FRONIUS_STEP_SLOW;
        else
            correction = delta;
    }
    else if (grid > FRONIUS_GRID_MARGIN_W) {
        // Cloud / insufficient PV: immediately reduce load
        correction = -FRONIUS_STEP_FAST;
    }

    simulatedDimmer += correction;

    if (simulatedDimmer < 0)
        simulatedDimmer = 0;
    if (simulatedDimmer > FRONIUS_MAX_DIMMER)
        simulatedDimmer = FRONIUS_MAX_DIMMER;

    heaterPower = (FRONIUS_HEATER_POWER_W * simulatedDimmer) / 100;
    if (surplus > 0)
        exportLeft = surplus - heaterPower;

    gDisplayValues.dimmer = simulatedDimmer;

    Serial.println();
    Serial.println("========== FRONIUS ZERO GRID SIMU V8 ==========");
    Serial.printf("PV production : %d W\n", production);
    Serial.printf("Grid exchange : %d W\n", grid);
    Serial.printf("Heater max    : %d W\n", FRONIUS_HEATER_POWER_W);
    Serial.printf("Dimmer actual : %d %%\n", simulatedDimmer);
    Serial.printf("Target dimmer : %d %%\n", targetDimmer);
    Serial.printf("Heater power  : %d W\n", heaterPower);
    Serial.printf("Export left   : %d W\n", exportLeft);

    if (grid < -FRONIUS_GRID_MARGIN_W)
        Serial.println("Status        : SURPLUS PV");
    else if (grid > FRONIUS_GRID_MARGIN_W)
        Serial.println("Status        : IMPORT RESEAU");
    else
        Serial.println("Status        : ZERO GRID OK");

    Serial.printf("Correction    : %d %%\n", correction);
    Serial.println("Decision      : FAST ADAPTIVE POWER UPDATE");
    Serial.println("===============================================");

    lastGrid = grid;
}

#endif
