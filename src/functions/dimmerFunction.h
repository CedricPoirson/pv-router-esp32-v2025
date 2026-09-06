#ifndef DIMMER_FUNCTIONS
#define DIMMER_FUNCTIONS

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <math.h>
#include "../config/enums.h"
#include "../config/config.h"
#include "../functions/spiffsFunctions.h"
#include "../functions/Mqtt_http_Functions.h"

// Defaults are kept here so an existing private config.h continues to build.
#ifndef DIMMER_RATED_POWER_W
#define DIMMER_RATED_POWER_W 800.0f
#endif

#ifndef DIMMER_HTTP_TIMEOUT_MS
#define DIMMER_HTTP_TIMEOUT_MS 500
#endif

#ifndef DIMMER_KEEPALIVE_MS
#define DIMMER_KEEPALIVE_MS 60000UL
#endif

#ifndef FRONIUS_STALE_MS
#define FRONIUS_STALE_MS 5000UL
#endif

#if DIMMERLOCAL
// Dimmer library
#include <RBDdimmer.h>   // corrected library in RBDDimmer-master-corrected.rar

dimmerLamp dimmer_hard(outputPin, zerocross);
#endif

extern DisplayValues gDisplayValues;
extern Config config;

// Last command that was actually acknowledged by the remote dimmer.
// -1 forces an initial command on boot.
static int lastSuccessfulDimmerCommand = -1;
static uint32_t lastSuccessfulDimmerCommandMs = 0;
static bool dimmerFailsafeActive = true;

/**
 * Send an absolute POWER command to the remote RobotDyn dimmer.
 * Firmware 20260514 API: GET /?POWER=<0..100>
 *
 * No blocking delay is used: a failed dimmer must never stall the router loop.
 */
bool dimmer_change(char dimmerurl[15], int dimmerIDX, int dimmervalue) {
#if WIFI_ACTIVE == true
  dimmervalue = constrain(dimmervalue, 0, 100);

  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  HTTPClient dimmerHttp;
  dimmerHttp.setConnectTimeout(DIMMER_HTTP_TIMEOUT_MS);
  dimmerHttp.setTimeout(DIMMER_HTTP_TIMEOUT_MS);

  String path = "/?POWER=" + String(dimmervalue);
  if (!dimmerHttp.begin(dimmerurl, 80, path)) {
    return false;
  }

  const int httpCode = dimmerHttp.GET();
  dimmerHttp.end();

  if (httpCode < 200 || httpCode >= 300) {
    Serial.print(F("Dimmer HTTP command failed, code="));
    Serial.println(httpCode);
    return false;
  }

  lastSuccessfulDimmerCommand = dimmervalue;
  lastSuccessfulDimmerCommandMs = millis();

#if MQTT_CLIENT == true
  // MQTT is telemetry only. It is never required for regulation.
  Mqtt_send(String(dimmerIDX), String(dimmervalue));
#endif

  return true;
#else
  (void)dimmerurl;
  (void)dimmerIDX;
  (void)dimmervalue;
  return false;
#endif
}

/**
 * Autonomous closed-loop regulation using Fronius Site/P_Grid.
 *
 * Fronius convention used by the existing router:
 *   P_Grid > 0 : importing from the grid
 *   P_Grid < 0 : exporting surplus
 *
 * The controllable surplus is the power already sent to the heater minus
 * P_Grid. This makes the requested POWER percentage absolute while avoiding
 * the 0%/surplus oscillation that would occur if only -P_Grid were used once
 * the heater is already consuming energy.
 */
void dimmer() {
  gDisplayValues.change = 0;
  const uint32_t now = millis();

  const bool froniusFresh =
      gDisplayValues.froniusup &&
      gDisplayValues.froniusLastOkMs != 0 &&
      (uint32_t)(now - gDisplayValues.froniusLastOkMs) <= FRONIUS_STALE_MS;

  // Fail safe: loss/stale Fronius data, voluntary routing stop, or a dimmer
  // alarm/off state always drives the requested power to zero.
  const bool remoteSafetyStop =
      gDisplayValues.dimmerOnline &&
      (!gDisplayValues.dimmerOn || gDisplayValues.dimmerAlarm);

  if (!froniusFresh || !config.autonome || remoteSafetyStop) {
    gDisplayValues.dimmer = 0;

    const bool mustSendZero =
        lastSuccessfulDimmerCommand != 0 ||
        !dimmerFailsafeActive ||
        (uint32_t)(now - lastSuccessfulDimmerCommandMs) >= DIMMER_KEEPALIVE_MS;

    if (mustSendZero) {
      gDisplayValues.change = 1;
      dimmer_change(config.dimmer, config.IDXdimmer, 0);
#if DIMMERLOCAL
      dimmer_hard.setPower(0);
#endif
    }

    dimmerFailsafeActive = true;
    return;
  }

  dimmerFailsafeActive = false;

  // Use the last acknowledged command as the heater power already included in
  // the Fronius grid measurement. At first valid measurement, fall back to the
  // current requested value (normally 0 after boot).
  const int currentPct =
      (lastSuccessfulDimmerCommand >= 0)
          ? lastSuccessfulDimmerCommand
          : constrain(gDisplayValues.dimmer, 0, 100);

  const float currentDimmerW = DIMMER_RATED_POWER_W * currentPct / 100.0f;
  const float availableSurplusW = currentDimmerW - (float)gDisplayValues.watt;

  int requestedPct = (int)lroundf(availableSurplusW * 100.0f / DIMMER_RATED_POWER_W);
  requestedPct = constrain(requestedPct, 0, 100);

  // Preserve the existing configurable maximum-power safety limit.
  if (config.num_fuse > 0 && requestedPct > config.num_fuse) {
    requestedPct = config.num_fuse;
  }

  gDisplayValues.dimmer = requestedPct;

  const bool changedByAtLeastOnePct =
      lastSuccessfulDimmerCommand < 0 ||
      abs(requestedPct - lastSuccessfulDimmerCommand) >= 1;

  const bool keepaliveDue =
      lastSuccessfulDimmerCommandMs == 0 ||
      (uint32_t)(now - lastSuccessfulDimmerCommandMs) >= DIMMER_KEEPALIVE_MS;

  if (changedByAtLeastOnePct || keepaliveDue) {
    gDisplayValues.change = 1;
    const bool sent = dimmer_change(config.dimmer, config.IDXdimmer, requestedPct);

#if DIMMERLOCAL
    if (sent) {
      dimmer_hard.setPower(requestedPct);
    }
#endif
  }

#if DEBUG == true
  Serial.print(F("P_Grid W: "));
  Serial.print(gDisplayValues.watt);
  Serial.print(F(" | available W: "));
  Serial.print(availableSurplusW);
  Serial.print(F(" | POWER %: "));
  Serial.print(requestedPct);
  Serial.print(F(" | Fronius OK: "));
  Serial.println(froniusFresh ? F("yes") : F("no"));
#endif
}

#if DIMMERLOCAL
void Dimmer_setup() {
  dimmer_hard.begin(NORMAL_MODE, ON);
  dimmer_hard.setPower(0);
  serial_println("Dimmer started...");
}
#endif

#endif
