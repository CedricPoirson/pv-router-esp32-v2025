#ifndef FRONIUS_ZERO_GRID_H
#define FRONIUS_ZERO_GRID_H

// Experimental Fronius Zero Grid controller
// Simulation only - no dimmer command enabled yet.
// Goal: minimize grid import while maximizing self consumption.

#include "config/enums.h"

// Positive P_Grid = import from grid
// Negative P_Grid = export to grid

#define FRONIUS_GRID_TARGET_W 0
#define FRONIUS_GRID_MARGIN_W 20
#define FRONIUS_HEATER_POWER_W 2400
#define FRONIUS_MAX_DIMMER_STEP 2

extern DisplayValues gDisplayValues;

static int simulatedDimmer = 0;

void froniusZeroGridSimulation()
{
    int grid = (int)gDisplayValues.grid;
    int production = (int)gDisplayValues.production;
    int dimmer = gDisplayValues.dimmer;

    Serial.println();
    Serial.println("========== FRONIUS ZERO GRID SIMU ==========");
    Serial.print("PV production : ");
    Serial.print(production);
    Serial.println(" W");

    Serial.print("Grid exchange : ");
    Serial.print(grid);
    Serial.println(" W");

    Serial.print("Dimmer actual : ");
    Serial.print(dimmer);
    Serial.println(" %");

    if (grid < -FRONIUS_GRID_MARGIN_W) {
        int surplus = abs(grid);
        int target = (surplus * 100) / FRONIUS_HEATER_POWER_W;

        simulatedDimmer += FRONIUS_MAX_DIMMER_STEP;
        if (simulatedDimmer > target)
            simulatedDimmer = target;

        Serial.println("Status        : SURPLUS PV");
        Serial.print("Surplus       : ");
        Serial.print(surplus);
        Serial.println(" W");
        Serial.print("Target dimmer : ");
        Serial.print(target);
        Serial.println(" %");
        Serial.print("Step applied  : +");
        Serial.print(FRONIUS_MAX_DIMMER_STEP);
        Serial.println(" %");
    }
    else if (grid > FRONIUS_GRID_MARGIN_W) {
        Serial.println("Status        : IMPORT RESEAU");
        Serial.println("Decision      : DIMMER DOWN (simulation)");
    }
    else {
        Serial.println("Status        : ZERO GRID OK");
        Serial.println("Decision      : HOLD");
    }

    Serial.println("============================================");
}

#endif
