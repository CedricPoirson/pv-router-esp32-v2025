#ifndef FRONIUS_ZERO_GRID_H
#define FRONIUS_ZERO_GRID_H

// Fronius Zero Grid controller
// V14.2 - dynamic surplus ramp + asymmetric anti-import control
// Positive P_Grid = import from grid
// Negative P_Grid = export to grid

#include "config/enums.h"
#include <HTTPClient.h>
#include <WiFi.h>

// Real measured heater power at 100% dimmer. This value is deliberately the
// physical resistance power used by the router, not RobotDyn /config.charge.
#define FRONIUS_HEATER_POWER_W 800
#define FRONIUS_MAX_DIMMER 100

// Bias the controller slightly toward export so short load changes are less
// likely to become grid import. Accepted steady band: -25 W .. -5 W.
#define FRONIUS_GRID_TARGET_W -15
#define FRONIUS_GRID_DEADBAND_W 10

// Asymmetric control tuning.
// Grid import has priority over surplus capture. Small imports are corrected
// without large oscillations; a real appliance load causes an immediate
// physics-based heater reduction, with a small export reserve to compensate
// for Fronius/RobotDyn feedback latency.
#define DIMMER_IMPORT_FAST_W 80
#define DIMMER_IMPORT_EMERGENCY_W 250
#define DIMMER_IMPORT_FINE_STEP_PERCENT 8
#define DIMMER_IMPORT_RESERVE_W 25
#define DIMMER_IMPORT_EMERGENCY_RESERVE_W 50

// Dynamic surplus capture. Large export should be absorbed quickly, while the
// final approach to Zero Grid remains deliberately slow to avoid overshoot.
// A lead limit prevents HTTP commands from running too far ahead of the power
// actually reported by RobotDyn when its /state feedback is a little slower.
#define DIMMER_SURPLUS_TURBO_W 300
#define DIMMER_SURPLUS_MEDIUM_W 100
#define DIMMER_SURPLUS_TURBO_STEP_PERCENT 20
#define DIMMER_SURPLUS_MEDIUM_STEP_PERCENT 8
#define DIMMER_SURPLUS_FINE_STEP_PERCENT 3
#define DIMMER_SURPLUS_MAX_LEAD_PERCENT 30

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

static int dimmerPercentToHeaterWatts(int percent)
{
    percent = constrain(percent, 0, FRONIUS_MAX_DIMMER);
    return (FRONIUS_HEATER_POWER_W * percent) / 100;
}

static int heaterWattsToDimmerPercent(int watts)
{
    watts = constrain(watts, 0, FRONIUS_HEATER_POWER_W);
    return (watts * 100 + (FRONIUS_HEATER_POWER_W / 2)) /
           FRONIUS_HEATER_POWER_W;
}

bool sendDimmerPower(int power)
{
    power = constrain(power, 0, 100);

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
    const int commandedDimmer = constrain(gDisplayValues.dimmer, 0, FRONIUS_MAX_DIMMER);
    const unsigned long now = millis();

    int reportedDimmer = gDisplayValues.dimmerReported;
    if (reportedDimmer < 0) reportedDimmer = 0;
    if (reportedDimmer > 100) reportedDimmer = 100;

    const bool dimmerStateFresh =
        gDisplayValues.dimmerCommOk &&
        gDisplayValues.dimmerLastOkMs > 0 &&
        ((unsigned long)(now - gDisplayValues.dimmerLastOkMs) <= DIMMER_STATE_FRESH_MS);

    // P_Grid already includes the heater consumption. Use the power that is
    // physically applied by the 800 W resistance to calculate the absolute
    // heater power that would place the grid at our small export target.
    const int controlDimmer = dimmerStateFresh ? reportedDimmer : commandedDimmer;
    const int currentHeaterPower = dimmerPercentToHeaterWatts(controlDimmer);

    // The last command is important while RobotDyn is still moving toward it.
    // It prevents an import sample from raising the command again simply
    // because ACTUAL is still above/below a previous request.
    const int requestedDimmer =
        lastSentDimmer >= 0
            ? constrain(lastSentDimmer, 0, FRONIUS_MAX_DIMMER)
            : commandedDimmer;

    const int gridLow = FRONIUS_GRID_TARGET_W - FRONIUS_GRID_DEADBAND_W;
    const int gridHigh = FRONIUS_GRID_TARGET_W + FRONIUS_GRID_DEADBAND_W;

    int rawTargetHeaterPower =
        currentHeaterPower + (FRONIUS_GRID_TARGET_W - grid);
    rawTargetHeaterPower = constrain(rawTargetHeaterPower,
                                     0,
                                     FRONIUS_HEATER_POWER_W);

    const int rawTargetDimmer =
        heaterWattsToDimmerPercent(rawTargetHeaterPower);

    int targetDimmer = requestedDimmer;

    if (grid > gridHigh) {
        // GRID IMPORT: reducing the dimmer reduces grid consumption.
        // Never increase the requested heater power while the house imports.
        targetDimmer = min(rawTargetDimmer, requestedDimmer);

        if (grid >= DIMMER_IMPORT_FAST_W) {
            // Fast appliance-load path. Remove the measured import in one
            // Fronius sample and deliberately leave a small export reserve.
            // If the new appliance consumes more than the heater can shed,
            // this naturally commands POWER=0 immediately.
            const int reserveW =
                grid >= DIMMER_IMPORT_EMERGENCY_W
                    ? DIMMER_IMPORT_EMERGENCY_RESERVE_W
                    : DIMMER_IMPORT_RESERVE_W;

            int fastTargetHeaterPower =
                currentHeaterPower - grid - reserveW;
            fastTargetHeaterPower = constrain(fastTargetHeaterPower,
                                              0,
                                              FRONIUS_HEATER_POWER_W);

            const int fastTargetDimmer =
                heaterWattsToDimmerPercent(fastTargetHeaterPower);
            targetDimmer = min(targetDimmer, fastTargetDimmer);
        }
        else {
            // Close to zero, limit the correction to avoid a needless swing
            // deep into export while still prioritising removal of grid draw.
            const int minAllowed =
                max(0, requestedDimmer - DIMMER_IMPORT_FINE_STEP_PERCENT);
            if (targetDimmer < minAllowed)
                targetDimmer = minAllowed;
        }
    }
    else if (grid < gridLow) {
        // PV SURPLUS: increasing the dimmer increases consumption by about
        // 8 W per percentage point with the measured 800 W resistance.
        // Large surplus gets a fast ramp; close to Zero Grid we keep small
        // steps. rawTargetDimmer remains the physics-based upper objective.
        targetDimmer = max(rawTargetDimmer, requestedDimmer);

        const int surplusW = -grid;
        int maxStep = DIMMER_SURPLUS_FINE_STEP_PERCENT;

        if (surplusW >= DIMMER_SURPLUS_TURBO_W)
            maxStep = DIMMER_SURPLUS_TURBO_STEP_PERCENT;
        else if (surplusW >= DIMMER_SURPLUS_MEDIUM_W)
            maxStep = DIMMER_SURPLUS_MEDIUM_STEP_PERCENT;

        const int maxAllowedByStep =
            min(FRONIUS_MAX_DIMMER, requestedDimmer + maxStep);
        if (targetDimmer > maxAllowedByStep)
            targetDimmer = maxAllowedByStep;

        // RobotDyn feedback can lag the command by one or two control cycles.
        // Allow a useful head start, but do not queue a large hidden increase
        // that could suddenly turn into grid import when the dimmer catches up.
        if (dimmerStateFresh) {
            const int maxAllowedByFeedback =
                min(FRONIUS_MAX_DIMMER,
                    controlDimmer + DIMMER_SURPLUS_MAX_LEAD_PERCENT);
            if (targetDimmer > maxAllowedByFeedback)
                targetDimmer = maxAllowedByFeedback;
        }
    }
    // Inside the target band, keep the last requested value. Do not chase
    // every Fronius watt with another HTTP command.

    targetDimmer = constrain(targetDimmer, 0, FRONIUS_MAX_DIMMER);
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
