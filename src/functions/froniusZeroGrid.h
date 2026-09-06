#ifndef FRONIUS_ZERO_GRID_H
#define FRONIUS_ZERO_GRID_H

// Fronius Zero Grid controller
// V13.4 - RobotDyn firmware 20260514 integration + fail-safe watchdog
// Positive P_Grid = import from grid
// Negative P_Grid = export to grid

#include "config/enums.h"
#include <HTTPClient.h>
#include <WiFi.h>

#define FRONIUS_HEATER_POWER_W 800
#define FRONIUS_MAX_DIMMER 100

// Aim for a very small permanent export so short variations do not
// immediately become grid import. Accepted band: -20 W .. 0 W.
#define FRONIUS_GRID_TARGET_W -10
#define FRONIUS_GRID_DEADBAND_W 10

#define DIMMER_IP "192.168.100.29"
#define DIMMER_MIN_CHANGE 1
#define DIMMER_HTTP_TIMEOUT_MS 500UL
#define DIMMER_REFRESH_MS 60000UL
#define DIMMER_FAILSAFE_RETRY_MS 1000UL

// If the dimmer reports a value that stays far from the requested command,
// force an HTTP resend instead of waiting only for the periodic refresh.
#define DIMMER_SYNC_TOLERANCE_PERCENT 10
#define DIMMER_SYNC_MISMATCH_MS 15000UL
#define DIMMER_STATE_FRESH_MS 10000UL
#define FRONIUS_STALE_MS 4000UL

extern DisplayValues gDisplayValues;

static int lastSentDimmer = -1;
static unsigned long lastDimmerSendMs = 0;
static unsigned long dimmerMismatchSinceMs = 0;
static unsigned long lastFailsafeAttemptMs = 0;
static bool dimmerFailsafeActive = true;
static int lastZeroGridState = -1;

enum ZeroGridState {
    ZERO_GRID_HOLD = 0,
    ZERO_GRID_SURPLUS,
    ZERO_GRID_IMPORT,
    ZERO_GRID_LOAD_LIMITED
};

bool sendDimmerPower(int power)
{
    power = constrain(power, 0, 100);

    if (WiFi.status() != WL_CONNECTED)
        return false;

    HTTPClient http;
    const String url = String("http://") + DIMMER_IP + "/?POWER=" + String(power);

    http.setConnectTimeout(DIMMER_HTTP_TIMEOUT_MS);
    http.setTimeout(DIMMER_HTTP_TIMEOUT_MS);

    if (!http.begin(url))
        return false;

    const int httpCode = http.GET();
    http.end();

    const bool ok = httpCode >= 200 && httpCode < 300;
    if (!ok) {
        Serial.printf("[DIMMER] HTTP ERROR=%d POWER=%d\n", httpCode, power);
    }

    return ok;
}

// Fail-safe used for invalid/stale Fronius data, voluntary routing stop,
// or a safety condition reported by the dimmer. POWER=0 is sent immediately
// on entry, then retried every second after a failed request and refreshed
// every 60 s once acknowledged (RobotDyn AUTO_OFF is 5 minutes).
void froniusZeroGridFailsafe(const char *reason)
{
    const unsigned long now = millis();
    gDisplayValues.dimmer = 0;

    const bool firstEntry = !dimmerFailsafeActive;
    const bool zeroNotAcknowledged = lastSentDimmer != 0;
    const bool retryDue =
        lastFailsafeAttemptMs == 0 ||
        ((unsigned long)(now - lastFailsafeAttemptMs) >= DIMMER_FAILSAFE_RETRY_MS);
    const bool refreshDue =
        lastSentDimmer == 0 &&
        ((unsigned long)(now - lastDimmerSendMs) >= DIMMER_REFRESH_MS);

    if ((firstEntry || zeroNotAcknowledged || refreshDue) && retryDue) {
        lastFailsafeAttemptMs = now;

        if (sendDimmerPower(0)) {
            lastSentDimmer = 0;
            lastDimmerSendMs = now;
            Serial.printf("[FAILSAFE] POWER=0 (%s)\n", reason);
        }
    }

    dimmerMismatchSinceMs = 0;
    dimmerFailsafeActive = true;
}

// Name kept for compatibility with the existing Dimmer task.
void froniusZeroGridSimulation()
{
    dimmerFailsafeActive = false;
    lastFailsafeAttemptMs = 0;

    const int grid = (int)gDisplayValues.grid;
    const int commandedDimmer = gDisplayValues.dimmer;
    const unsigned long now = millis();

    int reportedDimmer = gDisplayValues.dimmerReported;
    if (reportedDimmer < 0) reportedDimmer = 0;
    if (reportedDimmer > 100) reportedDimmer = 100;

    const bool dimmerStateFresh =
        gDisplayValues.dimmerCommOk &&
        gDisplayValues.dimmerLastOkMs > 0 &&
        ((unsigned long)(now - gDisplayValues.dimmerLastOkMs) <= DIMMER_STATE_FRESH_MS);

    // P_Grid already contains the real heater consumption. Therefore the
    // incremental controller must start from the heater power that is really
    // applied, not from an old command that may still be waiting to sync.
    const int controlDimmer = dimmerStateFresh ? reportedDimmer : commandedDimmer;

    int targetDimmer = controlDimmer;
    int targetHeaterPower = (FRONIUS_HEATER_POWER_W * controlDimmer) / 100;
    int powerCorrection = 0;

    const int gridLow = FRONIUS_GRID_TARGET_W - FRONIUS_GRID_DEADBAND_W;
    const int gridHigh = FRONIUS_GRID_TARGET_W + FRONIUS_GRID_DEADBAND_W;

    if (grid < gridLow || grid > gridHigh) {
        const int currentHeaterPower =
            (FRONIUS_HEATER_POWER_W * controlDimmer) / 100;

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

    const bool dimmerMismatch =
        dimmerStateFresh &&
        (abs(gDisplayValues.dimmer - reportedDimmer) > DIMMER_SYNC_TOLERANCE_PERCENT);

    bool mismatchResyncDue = false;
    if (dimmerMismatch) {
        if (dimmerMismatchSinceMs == 0) {
            dimmerMismatchSinceMs = now;
        }
        else if ((unsigned long)(now - dimmerMismatchSinceMs) >= DIMMER_SYNC_MISMATCH_MS) {
            mismatchResyncDue = true;
        }
    }
    else {
        dimmerMismatchSinceMs = 0;
    }

    const bool valueChanged =
        (lastSentDimmer < 0) ||
        (abs(gDisplayValues.dimmer - lastSentDimmer) >= DIMMER_MIN_CHANGE);
    const bool refreshDue =
        (lastSentDimmer >= 0) &&
        ((unsigned long)(now - lastDimmerSendMs) >= DIMMER_REFRESH_MS);

    if (valueChanged || refreshDue || mismatchResyncDue) {
        const int previousSentDimmer = lastSentDimmer;

        if (mismatchResyncDue) {
            Serial.printf("[DIMMER SYNC] mismatch CMD=%d ACTUAL=%d -> resend\n",
                          gDisplayValues.dimmer,
                          reportedDimmer);
        }

        const bool commandOk = sendDimmerPower(gDisplayValues.dimmer);

        if (commandOk) {
            lastSentDimmer = gDisplayValues.dimmer;
            lastDimmerSendMs = now;

            if (mismatchResyncDue)
                dimmerMismatchSinceMs = now;

            if (valueChanged) {
                Serial.printf("[DIMMER] %d%% -> %d%% (ACTUAL=%d%% GRID=%d W)\n",
                              previousSentDimmer < 0 ? commandedDimmer : previousSentDimmer,
                              gDisplayValues.dimmer,
                              controlDimmer,
                              grid);
            }
        }
    }

    int zeroGridState = ZERO_GRID_HOLD;

    if (grid < gridLow) {
        if (controlDimmer >= FRONIUS_MAX_DIMMER &&
            targetDimmer >= FRONIUS_MAX_DIMMER)
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
