#ifndef FRONIUS_ZERO_GRID_H
#define FRONIUS_ZERO_GRID_H

// Fronius Zero Grid controller
// V11 - real HTTP dimmer command

#include "config/enums.h"
#include <HTTPClient.h>

#define FRONIUS_GRID_MARGIN_W 20
#define FRONIUS_HEATER_POWER_W 800
#define FRONIUS_MAX_DIMMER 100
#define FRONIUS_STEP_FAST 20
#define FRONIUS_STEP_SLOW 5

#define DIMMER_IP "192.168.100.29"
#define DIMMER_MIN_CHANGE 2

extern DisplayValues gDisplayValues;

static int lastGrid = 0;
static int lastSentDimmer = -1;

void sendDimmerPower(int power)
{
    HTTPClient http;

    String url = String("http://") + DIMMER_IP + "/?POWER=" + String(power);

    http.begin(url);
    int httpCode = http.GET();

    Serial.print("Dimmer HTTP : ");
    Serial.print(httpCode);
    Serial.print(" POWER=");
    Serial.println(power);

    http.end();
}

void froniusZeroGridSimulation()
{
    int grid = (int)gDisplayValues.grid;
    int production = (int)gDisplayValues.production;
    int dimmer = gDisplayValues.dimmer;
    int targetDimmer = dimmer;
    int correction = 0;
    int surplus = 0;

    if (grid < -FRONIUS_GRID_MARGIN_W) {
        surplus = abs(grid);
        targetDimmer = (surplus * 100) / FRONIUS_HEATER_POWER_W;

        if (targetDimmer > FRONIUS_MAX_DIMMER)
            targetDimmer = FRONIUS_MAX_DIMMER;

        int delta = targetDimmer - dimmer;

        if (targetDimmer == FRONIUS_MAX_DIMMER && dimmer < FRONIUS_MAX_DIMMER)
            correction = FRONIUS_STEP_FAST;
        else if (abs(delta) > 40)
            correction = (delta > 0) ? FRONIUS_STEP_FAST : -FRONIUS_STEP_FAST;
        else if (abs(delta) > 10)
            correction = (delta > 0) ? FRONIUS_STEP_SLOW : -FRONIUS_STEP_SLOW;
        else
            correction = delta;
    }
    else if (grid > FRONIUS_GRID_MARGIN_W) {
        correction = -FRONIUS_STEP_FAST;
    }

    dimmer += correction;

    if (dimmer < 0) dimmer = 0;
    if (dimmer > FRONIUS_MAX_DIMMER) dimmer = FRONIUS_MAX_DIMMER;

    gDisplayValues.dimmer = dimmer;

    // Real dimmer command only when change is significant
    if (abs(gDisplayValues.dimmer - lastSentDimmer) >= DIMMER_MIN_CHANGE) {
        sendDimmerPower(gDisplayValues.dimmer);
        lastSentDimmer = gDisplayValues.dimmer;
    }

    Serial.println();
    Serial.println("========== FRONIUS ZERO GRID V11 ==========");
    Serial.printf("PV production : %d W\n", production);
    Serial.printf("Grid exchange : %d W\n", grid);
    Serial.printf("Heater max    : %d W\n", FRONIUS_HEATER_POWER_W);
    Serial.printf("Dimmer command: %d %%\n", gDisplayValues.dimmer);
    Serial.printf("Target dimmer : %d %%\n", targetDimmer);
    Serial.printf("Heater power  : %d W\n", (FRONIUS_HEATER_POWER_W * dimmer) / 100);

    if (grid < -FRONIUS_GRID_MARGIN_W)
        Serial.println("Status        : SURPLUS PV");
    else if (grid > FRONIUS_GRID_MARGIN_W)
        Serial.println("Status        : IMPORT RESEAU");
    else
        Serial.println("Status        : ZERO GRID OK");

    Serial.printf("Correction    : %d %%\n", correction);
    Serial.println("Decision      : REAL HTTP DIMMER COMMAND");
    Serial.println("===========================================");

    lastGrid = grid;
}

#endif
