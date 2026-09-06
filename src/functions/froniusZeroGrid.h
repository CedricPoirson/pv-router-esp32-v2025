#ifndef FRONIUS_ZERO_GRID_H
#define FRONIUS_ZERO_GRID_H

// Fronius Zero Grid controller
// V12.2 - direct closed-loop control from P_Grid + periodic dimmer refresh
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

// Even when the requested power does not change, resend it periodically.
// This keeps the remote ESP8266 synchronized if it restarts or if it has a
// communication watchdog, without spamming it on every Fronius sample.
#define DIMMER_REFRESH_MS 20000UL

extern DisplayValues gDisplayValues;

static int lastSentDimmer = -1;
static unsigned long lastDimmerSendMs = 0;

bool sendDimmerPower(int power)
{
    HTTPClient http;
    String url = String("http://") + DIMMER_IP + "/?POWER=" + String(power);

    http.setTimeout(DIMMER_HTTP_TIMEOUT_MS);
    http.begin(url);
    int httpCode = http.GET();
    http.end();

    Serial.printf("Dimmer HTTP : %d POWER=%d\n", httpCode, power);
    return httpCode == HTTP_CODE_OK;
}

// Name kept for compatibility with the existing Dimmer task.
void froniusZeroGridSimulation()
{
    const int grid = (int)gDisplayValues.grid;
    const int production = (int)gDisplayValues.production;
    const int dimmer = gDisplayValues.dimmer;

    int targetDimmer = dimmer;
    int targetHeaterPower = (FRONIUS_HEATER_POWER_W * dimmer) / 100;
    int powerCorrection = 0;

    const int gridLow = FRONIUS_GRID_TARGET_W - FRONIUS_GRID_DEADBAND_W;    // -20 W
    const int gridHigh = FRONIUS_GRID_TARGET_W + FRONIUS_GRID_DEADBAND_W;  //   0 W

    // Only correct outside the desired -20..0 W band.
    if (grid < gridLow || grid > gridHigh) {
        const int currentHeaterPower = (FRONIUS_HEATER_POWER_W * dimmer) / 100;

        // Increasing heater power moves P_Grid in the positive direction.
        // Example: grid=-600 W, target=-10 W => add about 590 W of load.
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

    const int correction = targetDimmer - dimmer;
    gDisplayValues.dimmer = targetDimmer;

    const unsigned long now = millis();
    const bool valueChanged =
        (lastSentDimmer < 0) ||
        (abs(gDisplayValues.dimmer - lastSentDimmer) >= DIMMER_MIN_CHANGE);
    const bool refreshDue =
        (lastSentDimmer >= 0) &&
        ((unsigned long)(now - lastDimmerSendMs) >= DIMMER_REFRESH_MS);

    bool commandSent = false;
    bool commandOk = true;

    if (valueChanged || refreshDue) {
        commandSent = true;
        commandOk = sendDimmerPower(gDisplayValues.dimmer);

        // Retry on the next control cycle if the ESP8266 did not answer.
        if (commandOk) {
            lastSentDimmer = gDisplayValues.dimmer;
            lastDimmerSendMs = now;
        }
    }

    Serial.println();
    Serial.println("========== FRONIUS ZERO GRID V12.2 ==========");
    Serial.printf("PV production : %d W\n", production);
    Serial.printf("Grid exchange : %d W\n", grid);
    Serial.printf("Grid target   : %d W (band %d..%d W)\n",
                  FRONIUS_GRID_TARGET_W, gridLow, gridHigh);
    Serial.printf("Heater max    : %d W\n", FRONIUS_HEATER_POWER_W);
    Serial.printf("Dimmer current: %d %%\n", dimmer);
    Serial.printf("Dimmer target : %d %%\n", targetDimmer);
    Serial.printf("Power target  : %d W\n", targetHeaterPower);
    Serial.printf("Correction    : %+d %% (%+d W requested)\n",
                  correction, powerCorrection);

    if (grid < gridLow) {
        if (dimmer >= FRONIUS_MAX_DIMMER && targetDimmer >= FRONIUS_MAX_DIMMER)
            Serial.println("Status        : LOAD LIMITED -> DIMMER MAX");
        else
            Serial.println("Status        : SURPLUS PV -> LOAD UP");
    }
    else if (grid > gridHigh) {
       Serial.println("Status        : IMPORT RESEAU -> LOAD DOWN");
    }
else {
    Serial.println("Status        : ZERO GRID OK -> HOLD");
}

    if (commandSent) {
        if (commandOk)
            Serial.printf("Dimmer output : HTTP OK (%s)\n",
                          valueChanged ? "new target" : "periodic refresh");
        else
            Serial.println("Dimmer output : HTTP ERROR - RETRY NEXT SAMPLE");
    }
    else {
        unsigned long ageMs = (lastSentDimmer >= 0) ? (now - lastDimmerSendMs) : 0;
        Serial.printf("Dimmer output : ACTIVE %d %% - no resend needed (age %lu s)\n",
                      lastSentDimmer,
                      ageMs / 1000UL);
    }

    Serial.println("=============================================");
}

#endif
