#ifndef FRONIUS_ZERO_GRID_H
#define FRONIUS_ZERO_GRID_H

// Experimental Fronius Zero Grid controller
// Simulation only - no dimmer command enabled yet.
// Goal: minimize grid import while maximizing self consumption.

// Positive P_Grid means import from grid on Fronius API.
// Negative P_Grid means export to grid.

#define FRONIUS_GRID_TARGET_W 0
#define FRONIUS_GRID_IMPORT_LIMIT_W 20
#define FRONIUS_GRID_DEADBAND_W 10

/*
Future simulation logic:

int grid = gDisplayValues.grid;

if (grid > FRONIUS_GRID_IMPORT_LIMIT_W) {
    // Reduce dimmer power quickly
}
else if (grid < -FRONIUS_GRID_DEADBAND_W) {
    // Increase dimmer power progressively
}
else {
    // Hold current dimmer value
}

Important:
- This module must not directly drive the triac yet.
- gDisplayValues.grid is the only Fronius Zero Grid reference.
- gDisplayValues.watt remains reserved for legacy code.

*/

#endif
