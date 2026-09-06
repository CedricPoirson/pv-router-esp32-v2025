#ifndef FRONIUS_ZERO_GRID_H
#define FRONIUS_ZERO_GRID_H

// Experimental Fronius Zero Grid controller
// Simulation stage - no real dimmer command enabled yet.
// Virtual dimmer follows calculated target for validation.

#include "config/enums.h"

// Positive P_Grid = import from grid
// Negative P_Grid = export to grid

#define FRONIUS_GRID_TARGET_W 0
#define FRONIUS_GRID_MARGIN_W 20
#define FRONIUS_HEATER_POWER_W 2400
#define FRONIUS_MAX_DIMMER_STEP 2
#define FRONIUS_IMPORT_STEP 5

extern DisplayValues gDisplayValues;

static int simulatedDimmer = 0;

void froniusZeroGridSimulation()
{
    int grid = (int)gDisplayValues.grid;
    int production = (int)gDisplayValues.production;

    int target = 0;

    if (grid < -FRONIUS_GRID_MARGIN_W) {
        int surplus = abs(grid);
        target = (surplus * 100) / FRONIUS_HEATER_POWER_W;
        if (target > 100)
            target = 100;

        if (simulatedDimmer < target) {
            simulatedDimmer += FRONIUS_MAX_DIMMER_STEP;
            if (simulatedDimmer > target)
                simulatedDimmer = target;
        }
    }
    else if (grid > FRONIUS_GRID_MARGIN_W) {
        target = 0;
        simulatedDimmer -= FRONIUS_IMPORT_STEP;
        if (simulatedDimmer < 0)
            simulatedDimmer = 0;
    }

    // Virtual feedback only for validation
    gDisplayValues.dimmer = simulatedDimmer;

    Serial.println();
    Serial.println("========== FRONIUS ZERO GRID SIMU ==========");

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

    Serial.print("Target dimmer : ");
    Serial.print(target);
    Serial.println(" %");

    Serial.println("Decision      : VIRTUAL DIMMER UPDATE");
    Serial.println("============================================");
}

#endif
