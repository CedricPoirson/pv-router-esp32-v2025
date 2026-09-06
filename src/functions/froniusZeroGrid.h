#ifndef FRONIUS_ZERO_GRID_H
#define FRONIUS_ZERO_GRID_H

// Experimental Fronius Zero Grid controller
// Simulation only - no dimmer command enabled yet.
// Goal: minimize grid import while maximizing self consumption.

#include "config/enums.h"

// Positive P_Grid means import from grid on Fronius API.
// Negative P_Grid means export to grid.

#define FRONIUS_GRID_TARGET_W 0
#define FRONIUS_GRID_IMPORT_LIMIT_W 20
#define FRONIUS_GRID_DEADBAND_W 10

extern DisplayValues gDisplayValues;

void froniusZeroGridSimulation()
{
    int grid = (int)gDisplayValues.grid;

    Serial.println("=== FRONIUS ZERO GRID SIMU ===");
    Serial.print("Grid : ");
    Serial.print(grid);
    Serial.println(" W");

    if (grid > FRONIUS_GRID_IMPORT_LIMIT_W) {
        Serial.println("Etat : IMPORT RESEAU");
        Serial.println("Action : DIMMER DOWN (simulation)");
    }
    else if (grid < -FRONIUS_GRID_DEADBAND_W) {
        Serial.println("Etat : SURPLUS PV");
        Serial.println("Action : DIMMER UP (simulation)");
    }
    else {
        Serial.println("Etat : ZONE CIBLE");
        Serial.println("Action : HOLD");
    }
}

#endif
