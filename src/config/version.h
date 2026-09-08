#ifndef PV_ROUTER_VERSION_H
#define PV_ROUTER_VERSION_H

// Project / firmware version. This is intentionally separate from the
// Zero Grid controller revision so UI/documentation/onboarding changes do not
// pretend to modify the regulation algorithm itself.
#define PV_ROUTER_FIRMWARE_VERSION "14.7"
#define PV_ROUTER_FIRMWARE_LABEL "V14.7"

// Regulation algorithm revision implemented in froniusZeroGrid.h.
#define PV_ROUTER_ZERO_GRID_VERSION "14.3"
#define PV_ROUTER_ZERO_GRID_LABEL "V14.3"

// Fronius interface used by this branch.
#define PV_ROUTER_FRONIUS_API_VERSION "Solar API v1"
#define PV_ROUTER_FRONIUS_POWERFLOW_PATH "/solar_api/v1/GetPowerFlowRealtimeData.fcgi"

#endif
