#ifndef FRONIUS_ZERO_GRID_H
#define FRONIUS_ZERO_GRID_H

// Experimental Fronius Zero Grid controller
// Simulation only - no dimmer command enabled yet.
// Goal: minimize grid import while maximizing self consumption.

#include "config/enums.h"

// Positive P_Grid = import from grid (Fronius API)
// Negative P_Grid = export to grid

#define FRONIUS_GRID_TARGET_W 0
#define FRONIUS_GRID_IMPORT_LIMIT_W 20
#define FRONIUS_GRID_DEADBAND_W 10

extern DisplayValues gDisplayValues;

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

    if (grid > FRONIUS_GRID_IMPORT_LIMIT_W) {
        Serial.println("Status        : IMPORT RESEAU");
        Serial.println("Decision      : DIMMER DOWN (simulation)");
    }
    else if (grid < -FRONIUS_GRID_DEADBAND_W) {
        Serial.println("Status        : SURPLUS PV");
        Serial.println("Decision      : DIMMER UP (simulation)");
    }
    else {
        Serial.println("Status        : ZONE ZERO GRID");
        Serial.println("Decision      : HOLD");
    }

    Serial.println("============================================");
}

#endif
