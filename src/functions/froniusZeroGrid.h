#ifndef FRONIUS_ZERO_GRID_H
#define FRONIUS_ZERO_GRID_H

// Fronius Zero Grid controller
// V14.3 - predictive physics control for fast cloud/load tracking
// Positive P_Grid = import from grid
// Negative P_Grid = export to grid

#include "config/enums.h"
#include <HTTPClient.h>
#include <WiFi.h>

// Proven defaults. The Web V2 configuration can override these at runtime;
// missing/old config.json files transparently keep these values.
#define FRONIUS_DEFAULT_HEATER_POWER_W 800
#define FRONIUS_DEFAULT_MAX_DIMMER 100
#define FRONIUS_DEFAULT_GRID_TARGET_W -15
#define FRONIUS_DEFAULT_GRID_DEADBAND_W 10

// Grid import has priority. A real appliance load is removed from the routed
// heater power immediately from the current Fronius sample, with a small
// additional export reserve to cover Fronius/RobotDyn feedback latency.
#define DIMMER_IMPORT_FAST_W 80
#define DIMMER_IMPORT_EMERGENCY_W 250
#define DIMMER_IMPORT_RESERVE_W 25
#define DIMMER_IMPORT_EMERGENCY_RESERVE_W 50

// V14.3 no longer uses arbitrary +3/+8/+20% ramps. The requested heater power
// is calculated directly from P_Grid and the configured physical heater load.
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
extern Config config;

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

// Runtime settings are intentionally clamped here as a second safety layer in
// addition to Web validation. This also protects imported/hand-edited JSON.
static int froniusConfiguredHeaterPowerW()
{
    const int value = config.heaterPowerW > 0
                          ? config.heaterPowerW
                          : FRONIUS_DEFAULT_HEATER_POWER_W;
    return constrain(value, 100, 5000);
}

static int froniusConfiguredMaxDimmer()
{
    const int value = config.dimmerMaxPercent > 0
                          ? config.dimmerMaxPercent
                          : FRONIUS_DEFAULT_MAX_DIMMER;
    return constrain(value, 10, 100);
}

static int froniusConfiguredGridTargetW()
{
    const int value = config.gridTargetW != 0
                          ? config.gridTargetW
                          : FRONIUS_DEFAULT_GRID_TARGET_W;
    return constrain(value, -200, 0);
}

static int froniusConfiguredGridDeadbandW()
{
    const int value = config.gridDeadbandW > 0
                          ? config.gridDeadbandW
                          : FRONIUS_DEFAULT_GRID_DEADBAND_W;
    return constrain(value, 2, 100);
}

static int froniusConfiguredMaxHeaterPowerW()
{
    return (froniusConfiguredHeaterPowerW() * froniusConfiguredMaxDimmer()) / 100;
}

static int dimmerPercentToHeaterWatts(int percent)
{
    percent = constrain(percent, 0, froniusConfiguredMaxDimmer());
    return (froniusConfiguredHeaterPowerW() * percent) / 100;
}

static int heaterWattsToDimmerPercent(int watts)
{
    const int heaterPowerW = froniusConfiguredHeaterPowerW();
    watts = constrain(watts, 0, froniusConfiguredMaxHeaterPowerW());
    const int percent = (watts * 100 + (heaterPowerW / 2)) / heaterPowerW;
    return constrain(percent, 0, froniusConfiguredMaxDimmer());
}

bool sendDimmerPower(int power)
{
    power = constrain(power, 0, froniusConfiguredMaxDimmer());

    if (WiFi.status() != WL_CONNECTED)
        return false;

    HTTPClient http;
    const String url = String("http://") + String(config.dimmer) +
                       "/?POWER=" + String(power);

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
    const int maxDimmer = froniusConfiguredMaxDimmer();
    const int maxHeaterPowerW = froniusConfiguredMaxHeaterPowerW();
    const int gridTargetW = froniusConfiguredGridTargetW();
    const int gridDeadbandW = froniusConfiguredGridDeadbandW();
    const int commandedDimmer = constrain(gDisplayValues.dimmer, 0, maxDimmer);
    const unsigned long now = millis();

    int reportedDimmer = gDisplayValues.dimmerReported;
    if (reportedDimmer < 0) reportedDimmer = 0;
    if (reportedDimmer > 100) reportedDimmer = 100;

    const bool dimmerStateFresh =
        gDisplayValues.dimmerCommOk &&
        gDisplayValues.dimmerLastOkMs > 0 &&
        ((unsigned long)(now - gDisplayValues.dimmerLastOkMs) <= DIMMER_STATE_FRESH_MS);

    // P_Grid already includes the heater consumption. Use the power actually
    // reported by RobotDyn when fresh; otherwise fall back to our command.
    const int controlDimmer = dimmerStateFresh ? reportedDimmer : commandedDimmer;
    const int currentHeaterPower = dimmerPercentToHeaterWatts(controlDimmer);

    // lastSentDimmer represents power already requested from RobotDyn even if
    // /state has not caught up yet. It is used by the predictive safety bound.
    const int requestedDimmer =
        lastSentDimmer >= 0
            ? constrain(lastSentDimmer, 0, maxDimmer)
            : commandedDimmer;
    const int requestedHeaterPower = dimmerPercentToHeaterWatts(requestedDimmer);

    const int gridLow = gridTargetW - gridDeadbandW;
    const int gridHigh = gridTargetW + gridDeadbandW;

    // Physics target: if heater power changes by X watts, P_Grid changes by
    // approximately +X watts. Therefore the heater power required to move the
    // grid directly to the configured target is:
    //   Pheater_target = Pheater_now + (Pgrid_target - Pgrid_now)
    int rawTargetHeaterPower =
        currentHeaterPower + (gridTargetW - grid);
    rawTargetHeaterPower = constrain(rawTargetHeaterPower,
                                     0,
                                     maxHeaterPowerW);

    const int rawTargetDimmer =
        heaterWattsToDimmerPercent(rawTargetHeaterPower);

    int targetDimmer = requestedDimmer;

    if (grid > gridHigh) {
        // GRID IMPORT: immediately shed the amount of heater power required by
        // the measured import. There is deliberately no downward ramp limit.
        targetDimmer = rawTargetDimmer;

        if (grid >= DIMMER_IMPORT_FAST_W) {
            const int reserveW =
                grid >= DIMMER_IMPORT_EMERGENCY_W
                    ? DIMMER_IMPORT_EMERGENCY_RESERVE_W
                    : DIMMER_IMPORT_RESERVE_W;

            int fastTargetHeaterPower =
                currentHeaterPower - grid - reserveW;
            fastTargetHeaterPower = constrain(fastTargetHeaterPower,
                                              0,
                                              maxHeaterPowerW);

            const int fastTargetDimmer =
                heaterWattsToDimmerPercent(fastTargetHeaterPower);
            targetDimmer = min(targetDimmer, fastTargetDimmer);
        }
    }
    else if (grid < gridLow) {
        // PV SURPLUS: go directly to the calculated physics target. This makes
        // the router follow fast cloud edges instead of waiting through fixed
        // percentage ramps.
        targetDimmer = rawTargetDimmer;

        // Predictive command bound: /state can lag our previous HTTP command.
        // Independently of that lag, never increase the command by more watts
        // than the export visible right now while preserving the target bias.
        const int safeAdditionalHeaterW =
            max(0, -grid + gridTargetW);
        const int maxSafeRequestedHeaterPower =
            constrain(requestedHeaterPower + safeAdditionalHeaterW,
                      0,
                      maxHeaterPowerW);
        const int maxSafeRequestedDimmer =
            heaterWattsToDimmerPercent(maxSafeRequestedHeaterPower);

        if (targetDimmer > requestedDimmer)
            targetDimmer = min(targetDimmer, maxSafeRequestedDimmer);
    }
    // Inside the target band, keep the last requested value. Do not chase
    // every Fronius watt with another HTTP command.

    targetDimmer = constrain(targetDimmer, 0, maxDimmer);
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
                Serial.printf("[DIMMER] %d%% -> %d%% (ACTUAL=%d%% GRID=%d W LOAD=%dW)\n",
                              previousSentDimmer < 0 ? commandedDimmer : previousSentDimmer,
                              gDisplayValues.dimmer,
                              controlDimmer,
                              grid,
                              currentHeaterPower);
            }
        }
    }

    int zeroGridState = ZERO_GRID_HOLD;

    if (grid < gridLow) {
        if (controlDimmer >= maxDimmer && targetDimmer >= maxDimmer)
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
                Serial.println("[ZERO] SURPLUS PV -> LOAD TRACK FAST");
                break;
            case ZERO_GRID_IMPORT:
                if (grid >= DIMMER_IMPORT_EMERGENCY_W)
                    Serial.println("[ZERO] IMPORT EMERGENCY -> LOAD DOWN FAST");
                else if (grid >= DIMMER_IMPORT_FAST_W)
                    Serial.println("[ZERO] IMPORT FAST -> LOAD DOWN");
                else
                    Serial.println("[ZERO] IMPORT RESEAU -> LOAD DOWN");
                break;
            default:
                Serial.printf("[ZERO] TARGET OK (%d..%d W)\n",
                              gridLow,
                              gridHigh);
                break;
        }

        lastZeroGridState = zeroGridState;
    }
}

#endif
