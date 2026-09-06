#ifndef FRONIUS_ZERO_GRID_H
#define FRONIUS_ZERO_GRID_H

// Experimental Fronius Zero Grid controller
// Simulation stage - no real dimmer command enabled yet.
// V4 adaptive regulation for fast PV variation.

#include "config/enums.h"

// Positive P_Grid = import from grid
// Negative P_Grid = export to grid

#define FRONIUS_GRID_MARGIN_W 50
#define FRONIUS_HEATER_POWER_W 2400
#define FRONIUS_MAX_DIMMER 100

extern DisplayValues gDisplayValues;

static int simulatedDimmer = 0;

void froniusZeroGridSimulation()
{
    int grid = (int)gDisplayValues.grid;
    int production = (int)gDisplayValues.production;
    int correction = 0;

    // Big PV surplus: react quickly
    if (grid < -FRONIUS_GRID_MARGIN_W) {
        int surplus = abs(grid);
        int wanted = (surplus * 100) / FRONIUS_HEATER_POWER_W;

        if (wanted > FRONIUS_MAX_DIMMER)
            wanted = FRONIUS_MAX_DIMMER;

        correction = wanted - simulatedDimmer;

        // Adaptive step: fast far away, soft near target
        if (abs(correction) > 20)
            correction = (correction > 0) ? 10 : -10;
        else if (abs(correction) > 5)
            correction = (correction > 0) ? 5 : -5;
    }
    // Cloud or sudden consumption: cut faster to avoid grid import
    else if (grid > FRONIUS_GRID_MARGIN_W) {
        correction = -8;
    }

    simulatedDimmer += correction;

    if (simulatedDimmer < 0)
        simulatedDimmer = 0;
    if (simulatedDimmer > FRONIUS_MAX_DIMMER)
        simulatedDimmer = FRONIUS_MAX_DIMMER;

    gDisplayValues.dimmer = simulatedDimmer;

    Serial.println();
    Serial.println("========== FRONIUS ZERO GRID SIMU V4 ==========");
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
    Serial.println("Decision      : ADAPTIVE VIRTUAL DIMMER UPDATE");
    Serial.println("===============================================");
}

#endif
