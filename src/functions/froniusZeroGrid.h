#ifndef FRONIUS_ZERO_GRID_H
#define FRONIUS_ZERO_GRID_H

// Experimental Fronius Zero Grid controller
// Simulation stage - no real dimmer command enabled yet.
// V3 regulation simulation with proportional correction.

#include "config/enums.h"

// Positive P_Grid = import from grid
// Negative P_Grid = export to grid

#define FRONIUS_GRID_TARGET_W 0
#define FRONIUS_GRID_MARGIN_W 20
#define FRONIUS_HEATER_POWER_W 2400
#define FRONIUS_MAX_DIMMER_STEP 2
#define FRONIUS_IMPORT_STEP 2

extern DisplayValues gDisplayValues;

static int simulatedDimmer = 0;

void froniusZeroGridSimulation()
{
    int grid = (int)gDisplayValues.grid;
    int production = (int)gDisplayValues.production;
    int target = simulatedDimmer;
    int correction = 0;

    // Surplus PV: increase load progressively
    if (grid < -FRONIUS_GRID_MARGIN_W) {
        int surplus = abs(grid);
        int wanted = (surplus * 100) / FRONIUS_HEATER_POWER_W;

        if (wanted > 100)
            wanted = 100;

        correction = wanted - simulatedDimmer;

        if (correction > FRONIUS_MAX_DIMMER_STEP)
            correction = FRONIUS_MAX_DIMMER_STEP;

        simulatedDimmer += correction;
    }
    // Grid import: reduce load progressively
    else if (grid > FRONIUS_GRID_MARGIN_W) {
        correction = -FRONIUS_IMPORT_STEP;
        simulatedDimmer += correction;
    }

    if (simulatedDimmer < 0)
        simulatedDimmer = 0;
    if (simulatedDimmer > 100)
        simulatedDimmer = 100;

    gDisplayValues.dimmer = simulatedDimmer;

    Serial.println();
    Serial.println("========== FRONIUS ZERO GRID SIMU V3 ==========");

    Serial.print("PV production : ");
    Serial.print(production);
    Serial.println(" W");

    Serial.print("Grid exchange : ");
    Serial.print(grid);
    Serial.println(" W");

    Serial.print("Dimmer actual : ");
    Serial.print(simulatedDimmer);
    Serial.println(" %");

    if (grid < -FRONIUS_GRID_MARGIN_W) {
        Serial.println("Status        : SURPLUS PV");
        Serial.print("Surplus       : ");
        Serial.print(abs(grid));
        Serial.println(" W");
    }
    else if (grid > FRONIUS_GRID_MARGIN_W) {
        Serial.println("Status        : IMPORT RESEAU");
    }
    else {
        Serial.println("Status        : ZERO GRID OK");
    }

    Serial.print("Correction    : ");
    Serial.print(correction);
    Serial.println(" %");

    Serial.println("Decision      : VIRTUAL DIMMER UPDATE");
    Serial.println("===============================================");
}

#endif
