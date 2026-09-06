#ifndef FRONIUS_ZERO_GRID_H
#define FRONIUS_ZERO_GRID_H

// Experimental Fronius zero-grid controller
// This module is intentionally not enabled yet.
// Target: keep grid exchange around -20W with a +/-20W deadband.

#define FRONIUS_GRID_TARGET_W -20
#define FRONIUS_GRID_DEADBAND_W 20

/*
 Future logic:

 if (gDisplayValues.grid > FRONIUS_GRID_TARGET_W + FRONIUS_GRID_DEADBAND_W)
     decrease dimmer;

 if (gDisplayValues.grid < FRONIUS_GRID_TARGET_W - FRONIUS_GRID_DEADBAND_W)
     increase dimmer;

*/

#endif
