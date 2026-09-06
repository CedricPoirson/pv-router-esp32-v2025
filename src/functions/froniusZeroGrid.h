#ifndef FRONIUS_ZERO_GRID_H
#define FRONIUS_ZERO_GRID_H

// Fronius Zero Grid controller
// V13.1 - V12.2 regulation with concise event-based serial logging
// Positive P_Grid = import from grid
// Negative P_Grid = export to grid

#include "config/enums.h"
#include <HTTPClient.h>

#define FRONIUS_HEATER_POWER_W 800
#define FRONIUS_MAX_DIMMER 100

// Aim for a very small permanent export so short variations do not
// immediately become grid import. Accepted band: -20 W .. 0 W.
#define FRONIUS_GRID_TARGET_W -10
#define FRONIUS_GRID_DEADBAND_W 10

#define DIMMER_IP "192.168.100.29"
#define DIMMER_MIN_CHANGE 1
#define DIMMER_HTTP_TIMEOUT_MS 1500
#define DIMMER_REFRESH_MS 20000UL

extern DisplayValues gDisplayValues;

static int lastSentDimmer = -1;
static unsigned long lastDimmerSendMs = 0;
static int lastZeroGridState = -1;

enum ZeroGridState {
    ZERO_GRID_HOLD = 0,
    ZERO_GRID_SURPLUS,
    ZERO_GRID_IMPORT,
    ZERO_GRID_LOAD_LIMITED
};

bool sendDimmerPower(int power)
{
    HTTPClient http;
    String url = String("http://") + DIMMER_IP + "/?POWER=" + String(power);

    http.setTimeout(DIMMER_HTTP_TIMEOUT_MS);
    http.begin(url);
    int httpCode = http.GET();
    http.end();

    if (httpCode != HTTP_CODE_OK) {
        Serial.printf("[DIMMER] HTTP ERROR=%d POWER=%d\n", httpCode, power);
    }

    return httpCode == HTTP_CODE_OK;
}

// Name kept for compatibility with the existing Dimmer task.
void froniusZeroGridSimulation()
{
    const int grid = (int)gDisplayValues.grid;
    const int dimmer = gDisplayValues.dimmer;

    int targetDimmer = dimmer;
    int targetHeaterPower = (FRONIUS_HEATER_POWER_W * dimmer) / 100;
    int powerCorrection = 0;

    const int gridLow = FRONIUS_GRID_TARGET_W - FRONIUS_GRID_DEADBAND_W;
    const int gridHigh = FRONIUS_GRID_TARGET_W + FRONIUS_GRID_DEADBAND_W;

    if (grid < gridLow || grid > gridHigh) {
        const int currentHeaterPower = (FRONIUS_HEATER_POWER_W * dimmer) / 100;

        powerCorrection = FRONIUS_GRID_TARGET_W - grid;
        targetHeaterPower = currentHeaterPower + powerCorrection;

        if (targetHeaterPower < 0)
            targetHeaterPower = 0;
        if (targetHeaterPower > FRONIUS_HEATER_POWER_W)
            targetHeaterPower = FRONIUS_HEATER_POWER_W;

        targetDimmer = (targetHeaterPower * 100 + (FRONIUS_HEATER_POWER_W / 2)) /
                       FRONIUS_HEATER_POWER_W;
    }

    if (targetDimmer < 0) targetDimmer = 0;
    if (targetDimmer > FRONIUS_MAX_DIMMER) targetDimmer = FRONIUS_MAX_DIMMER;

    gDisplayValues.dimmer = targetDimmer;

    const unsigned long now = millis();
    const bool valueChanged =
        (lastSentDimmer < 0) ||
        (abs(gDisplayValues.dimmer - lastSentDimmer) >= DIMMER_MIN_CHANGE);
    const bool refreshDue =
        (lastSentDimmer >= 0) &&
        ((unsigned long)(now - lastDimmerSendMs) >= DIMMER_REFRESH_MS);

    if (valueChanged || refreshDue) {
        const int previousSentDimmer = lastSentDimmer;
        const bool commandOk = sendDimmerPower(gDisplayValues.dimmer);

        if (commandOk) {
            lastSentDimmer = gDisplayValues.dimmer;
            lastDimmerSendMs = now;

            if (valueChanged) {
                Serial.printf("[DIMMER] %d%% -> %d%% (GRID=%d W)\n",
                              previousSentDimmer < 0 ? dimmer : previousSentDimmer,
                              gDisplayValues.dimmer,
                              grid);
            }
        }
    }

    int zeroGridState = ZERO_GRID_HOLD;

    if (grid < gridLow) {
        if (dimmer >= FRONIUS_MAX_DIMMER && targetDimmer >= FRONIUS_MAX_DIMMER)
            zeroGridState = ZERO_GRID_LOAD_LIMITED;
        else
            zeroGridState = ZERO_GRID_SURPLUS;
    }
    else if (grid > gridHigh) {
        zeroGridState = ZERO_GRID_IMPORT;
    }

    if (zeroGridState != lastZeroGridState) {
        switch (zeroGridState) {
            case ZERO_GRID_LOAD_LIMITED:
                Serial.printf("[ZERO] LOAD LIMITED - DIMMER MAX, export=%d W\n",
                              grid < 0 ? -grid : 0);
                break;
            case ZERO_GRID_SURPLUS:
                Serial.println("[ZERO] SURPLUS PV -> LOAD UP");
                break;
            case ZERO_GRID_IMPORT:
                Serial.println("[ZERO] IMPORT RESEAU -> LOAD DOWN");
                break;
            default:
                Serial.println("[ZERO] TARGET OK (-20..0 W)");
                break;
        }

        lastZeroGridState = zeroGridState;
    }
}

#endif
