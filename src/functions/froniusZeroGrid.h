#ifndef FRONIUS_ZERO_GRID_H
#define FRONIUS_ZERO_GRID_H

// Experimental Fronius Zero Grid controller
// Simulation stage - no real dimmer command enabled yet.
// V6 adaptive regulation for 800W heater.

#include "config/enums.h"

// Positive P_Grid = import from grid
// Negative P_Grid = export to grid

#define FRONIUS_GRID_MARGIN_W 20
#define FRONIUS_HEATER_POWER_W 800
#define FRONIUS_MAX_DIMMER 100

extern DisplayValues gDisplayValues;

static int simulatedDimmer = 0;

void froniusZeroGridSimulation()
{
    int grid = (int)gDisplayValues.grid;
    int production = (int)gDisplayValues.production;
    int correction = 0;
    int targetPower = 0;
    int wanted = simulatedDimmer;

    int surplus = 0;

    if (grid < -FRONIUS_GRID_MARGIN_W) {
        surplus = abs(grid);
        targetPower = surplus;
        wanted = (surplus * 100) / FRONIUS_HEATER_POWER_W;

        if (wanted > FRONIUS_MAX_DIMMER)
            wanted = FRONIUS_MAX_DIMMER;

        correction = wanted - simulatedDimmer;

        if (abs(correction) > 30)
            correction = (correction > 0) ? 10 : -10;
        else if (abs(correction) > 5)
            correction = (correction > 0) ? 3 : -3;
        else
            correction = (correction > 0) ? 1 : (correction < 0 ? -1 : 0);
    }
    else if (grid > FRONIUS_GRID_MARGIN_W) {
        // Fast protection against cloud/import
        correction = -10;
    }

    simulatedDimmer += correction;

    if (simulatedDimmer < 0)
        simulatedDimmer = 0;
    if (simulatedDimmer > FRONIUS_MAX_DIMMER)
        simulatedDimmer = FRONIUS_MAX_DIMMER;

    gDisplayValues.dimmer = simulatedDimmer;

    Serial.println();
    Serial.println("========== FRONIUS ZERO GRID SIMU V6 ==========");
    Serial.print("PV production : ");
    Serial.print(production);
    Serial.println(" W");
    Serial.print("Grid exchange : ");
    Serial.print(grid);
    Serial.println(" W");
    Serial.print("Load power    : ");
    Serial.print(FRONIUS_HEATER_POWER_W);
    Serial.println(" W");
    Serial.print("Target power  : ");
    Serial.print(targetPower);
    Serial.println(" W");
    Serial.print("Dimmer actual : ");
    Serial.print(simulatedDimmer);
    Serial.println(" %");

    if (grid < -FRONIUS_GRID_MARGIN_W) {
        Serial.println("Status        : SURPLUS PV");
        Serial.print("Surplus       : ");
        Serial.print(surplus);
        Serial.println(" W");
        Serial.print("Target dimmer : ");
        Serial.print(wanted);
        Serial.println(" %");
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
