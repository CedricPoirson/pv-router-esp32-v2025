#ifndef TASK_GET_TEMP
#define TASK_GET_TEMP

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "../config/config.h"
#include "../config/enums.h"
#include "../functions/Mqtt_http_Functions.h"

extern DisplayValues gDisplayValues;
extern Config config;

#ifdef TTGO
extern volatile bool gDisplayForceRefresh;
#endif

#define DIMMER_STATE_POLL_SYNC_MS          5000UL
#define DIMMER_STATE_POLL_CATCHUP_MS       2000UL
#define DIMMER_STATE_POLL_ECO_MS          30000UL
#define DIMMER_STATE_HTTP_TIMEOUT_MS        500UL
#define DIMMER_CONFIG_POLL_MS             30000UL
#define DIMMER_CONFIG_RETRY_MS            10000UL
#define DIMMER_CONFIG_POLL_ECO_MS        300000UL
#define DIMMER_ECO_AFTER_FRONIUS_OFF_MS  600000UL
#define DIMMER_ECS_FALLBACK_HYSTERESIS_C   2.0f

void GetDImmerTemp(void * parameter){
  (void)parameter;

  bool linkStateKnown = false;
  bool previousLinkOk = false;
  bool configKnown = false;
  unsigned long lastConfigAttemptMs = 0;
  unsigned long froniusOfflineSinceMs = 0;
  bool previousEcoMode = false;

  // -1 means the RobotDyn trigger has not been read yet. The normal path uses
  // the exact RobotDyn percentage hysteresis. The former fixed 2 C hysteresis
  // is kept only as a conservative fallback while /config is unavailable.
  gDisplayValues.dimmerTriggerPercent = -1;
  gDisplayValues.dimmerReleaseTemp = 0.0f;
  gDisplayValues.dimmerTempLimitActive = false;

  // Local ECS temperature latch. Once the maximum is reached, routing remains
  // stopped until the water has cooled to the same release threshold used by
  // RobotDyn. The RobotDyn firmware stores both maxtemp and trigger as ints and
  // therefore evaluates: maxtemp - ((maxtemp * trigger) / 100) with integer
  // truncation. We deliberately mirror that exact behaviour here.
  bool ecsTempLimitActive = false;
  bool previousIgnoredRemoteTempAlarm = false;
  bool previousOtherRemoteAlarm = false;
  String previousOtherRemoteAlert = "";

  for (;;) {
    const unsigned long now = millis();

    if (gDisplayValues.froniusup) {
      froniusOfflineSinceMs = 0;
    }
    else if (froniusOfflineSinceMs == 0) {
      froniusOfflineSinceMs = now;
    }

    const bool ecoMode =
        !gDisplayValues.froniusup &&
        froniusOfflineSinceMs != 0 &&
        (unsigned long)(now - froniusOfflineSinceMs) >= DIMMER_ECO_AFTER_FRONIUS_OFF_MS;

    if (ecoMode != previousEcoMode) {
      Serial.printf("[DIMMER] Eco polling %s\n", ecoMode ? "ON" : "OFF");
      previousEcoMode = ecoMode;
    }

    unsigned long nextPollMs =
        ecoMode ? DIMMER_STATE_POLL_ECO_MS
                : DIMMER_STATE_POLL_CATCHUP_MS;
    bool currentLinkOk = false;
    String errorReason = "unknown";

    // /config contains the real normal ECS maximum temperature (maxtemp) and
    // RobotDyn thermal hysteresis (trigger). While config is still unknown,
    // keep the normal 10 s retry for safety. Once known, slow it to 5 minutes
    // after a prolonged Fronius outage (typically at night).
    const unsigned long configInterval =
        !configKnown
            ? DIMMER_CONFIG_RETRY_MS
            : (ecoMode ? DIMMER_CONFIG_POLL_ECO_MS
                       : DIMMER_CONFIG_POLL_MS);

    if (lastConfigAttemptMs == 0 ||
        (unsigned long)(now - lastConfigAttemptMs) >= configInterval) {
      lastConfigAttemptMs = now;

      HTTPClient httpConfig;
      httpConfig.setConnectTimeout(DIMMER_STATE_HTTP_TIMEOUT_MS);
      httpConfig.setTimeout(DIMMER_STATE_HTTP_TIMEOUT_MS);

      if (httpConfig.begin(String(config.dimmer), 80, "/config")) {
        const int configHttpCode = httpConfig.GET();

        if (configHttpCode == HTTP_CODE_OK) {
          const String configPayload = httpConfig.getString();
          StaticJsonDocument<768> configDoc;
          const DeserializationError configError =
              deserializeJson(configDoc, configPayload);

          if (!configError && configDoc.containsKey("maxtemp")) {
            const int remoteMaxTemp = configDoc["maxtemp"] | 0;
            const int remoteTrigger =
                configDoc.containsKey("trigger")
                    ? (configDoc["trigger"] | -1)
                    : -1;
            const bool triggerValid =
                remoteTrigger >= 0 && remoteTrigger <= 100;

            if (remoteMaxTemp > 0 && remoteMaxTemp <= 100) {
              const bool maxChanged =
                  gDisplayValues.dimmerMaxTemp != remoteMaxTemp;
              const bool triggerChanged =
                  triggerValid &&
                  gDisplayValues.dimmerTriggerPercent != remoteTrigger;
              const bool firstValidConfig = !configKnown;

              gDisplayValues.dimmerMaxTemp = remoteMaxTemp;
              if (triggerValid)
                gDisplayValues.dimmerTriggerPercent = remoteTrigger;

              const bool triggerKnown =
                  gDisplayValues.dimmerTriggerPercent >= 0;
              float releaseTemp =
                  (float)gDisplayValues.dimmerMaxTemp -
                  DIMMER_ECS_FALLBACK_HYSTERESIS_C;
              if (triggerKnown) {
                const int releaseDeltaC =
                    (gDisplayValues.dimmerMaxTemp *
                     gDisplayValues.dimmerTriggerPercent) / 100;
                releaseTemp =
                    (float)(gDisplayValues.dimmerMaxTemp - releaseDeltaC);
              }
              if (releaseTemp < 0.0f) releaseTemp = 0.0f;
              gDisplayValues.dimmerReleaseTemp = releaseTemp;
              configKnown = true;

              if (firstValidConfig || maxChanged || triggerChanged) {
                if (triggerKnown) {
                  Serial.printf("[DIMMER] CONFIG OK MAX=%d C TRIGGER=%d%% RELEASE=%.1f C\n",
                                gDisplayValues.dimmerMaxTemp,
                                gDisplayValues.dimmerTriggerPercent,
                                gDisplayValues.dimmerReleaseTemp);
                }
                else {
                  Serial.printf("[DIMMER] CONFIG OK MAX=%d C TRIGGER=-- RELEASE=%.1f C (fallback)\n",
                                gDisplayValues.dimmerMaxTemp,
                                gDisplayValues.dimmerReleaseTemp);
                }
              }
            }
          }
        }

        httpConfig.end();
      }
    }

    HTTPClient httpdimmer;
    httpdimmer.setConnectTimeout(DIMMER_STATE_HTTP_TIMEOUT_MS);
    httpdimmer.setTimeout(DIMMER_STATE_HTTP_TIMEOUT_MS);

    if (httpdimmer.begin(String(config.dimmer), 80, "/state")) {
      const int httpResponseCode = httpdimmer.GET();

      if (httpResponseCode == HTTP_CODE_OK) {
        const String payload = httpdimmer.getString();
        StaticJsonDocument<1024> doc;
        const DeserializationError error = deserializeJson(doc, payload);

        if (!error && doc.containsKey("dimmer") && doc.containsKey("onoff")) {
          gDisplayValues.dimmerReported = constrain(doc["dimmer"] | 0, 0, 100);
          gDisplayValues.dimmerCommandReported = constrain(doc["commande"] | 0, 0, 100);

          String ecsTemp = "";
          if (doc["dallas0"].is<const char*>()) {
            ecsTemp = doc["dallas0"].as<String>();
          }
          else if (doc["temperature"].is<const char*>()) {
            ecsTemp = doc["temperature"].as<String>();
          }
          gDisplayValues.temperature = ecsTemp;

          gDisplayValues.dimmerPower = doc["power"] | 0.0f;
          gDisplayValues.dimmerPtotal = doc["Ptotal"] | 0.0f;
          gDisplayValues.dimmerRssi = doc["RSSI"] | -127;
          gDisplayValues.dimmerOn = doc["onoff"] | false;

          gDisplayValues.dimmerAlert = String((const char *)(doc["alerte"] | ""));
          gDisplayValues.dimmerVersion = String((const char *)(doc["version"] | ""));

          const bool remoteAlarm =
              gDisplayValues.dimmerAlert.length() > 0 &&
              !gDisplayValues.dimmerAlert.equalsIgnoreCase("RAS");

          const bool remoteTemperatureAlarm =
              remoteAlarm &&
              (gDisplayValues.dimmerAlert.indexOf("Temp") >= 0 ||
               gDisplayValues.dimmerAlert.indexOf("temp") >= 0);

          const float waterTemp = gDisplayValues.temperature.toFloat();
          const int effectiveMaxTemp =
              gDisplayValues.dimmerMaxTemp > 0
                  ? gDisplayValues.dimmerMaxTemp
                  : config.tmax;
          const bool temperatureKnown =
              waterTemp > 0.0f && effectiveMaxTemp > 0;

          float releaseTemp =
              (float)effectiveMaxTemp - DIMMER_ECS_FALLBACK_HYSTERESIS_C;
          if (gDisplayValues.dimmerTriggerPercent >= 0) {
            const int releaseDeltaC =
                (effectiveMaxTemp * gDisplayValues.dimmerTriggerPercent) / 100;
            releaseTemp = (float)(effectiveMaxTemp - releaseDeltaC);
          }
          if (releaseTemp < 0.0f) releaseTemp = 0.0f;
          gDisplayValues.dimmerReleaseTemp = releaseTemp;

          const bool previousTempLimit = ecsTempLimitActive;

          if (temperatureKnown) {
            if (!ecsTempLimitActive) {
              if (waterTemp >= (float)effectiveMaxTemp ||
                  (remoteTemperatureAlarm && waterTemp > releaseTemp)) {
                ecsTempLimitActive = true;
              }
            }
            else if (waterTemp <= releaseTemp) {
              ecsTempLimitActive = false;
            }
          }
          else if (remoteTemperatureAlarm) {
            ecsTempLimitActive = true;
          }

          gDisplayValues.dimmerTempLimitActive = ecsTempLimitActive;

          if (!previousTempLimit && ecsTempLimitActive) {
            Serial.printf("[DIMMER] ECS MAX TEMP REACHED %.1f/%d C -> POWER=0\n",
                          waterTemp,
                          effectiveMaxTemp);
          }
          else if (previousTempLimit && !ecsTempLimitActive) {
            Serial.printf("[DIMMER] ECS TEMP RELEASED %.1f C (MAX=%d C, restart<=%.1f C, trigger=%d%%)\n",
                          waterTemp,
                          effectiveMaxTemp,
                          releaseTemp,
                          gDisplayValues.dimmerTriggerPercent);
          }

          const bool ignoredRemoteTemperatureAlarm =
              remoteTemperatureAlarm &&
              temperatureKnown &&
              !ecsTempLimitActive;

          if (ignoredRemoteTemperatureAlarm &&
              !previousIgnoredRemoteTempAlarm) {
            Serial.printf("[DIMMER] STALE TEMP ALARM IGNORED %.1f C < restart %.1f C (MAX=%d C)\n",
                          waterTemp,
                          releaseTemp,
                          effectiveMaxTemp);
          }
          previousIgnoredRemoteTempAlarm = ignoredRemoteTemperatureAlarm;

          const bool otherRemoteAlarm =
              remoteAlarm && !remoteTemperatureAlarm;

          if (otherRemoteAlarm &&
              (!previousOtherRemoteAlarm ||
               previousOtherRemoteAlert != gDisplayValues.dimmerAlert)) {
            Serial.printf("[DIMMER] ALARM: %s\n",
                          gDisplayValues.dimmerAlert.c_str());
          }
          previousOtherRemoteAlarm = otherRemoteAlarm;
          previousOtherRemoteAlert =
              otherRemoteAlarm ? gDisplayValues.dimmerAlert : "";

          gDisplayValues.dimmerAlarm =
              otherRemoteAlarm || ecsTempLimitActive;

          gDisplayValues.dimmerCommOk = true;
          gDisplayValues.dimmerLastOkMs = millis();
          currentLinkOk = true;

          int commandedDimmer = constrain(gDisplayValues.dimmer, 0, 100);
          const bool synced =
              abs(commandedDimmer - gDisplayValues.dimmerReported) <= 2;
          nextPollMs = ecoMode
                           ? DIMMER_STATE_POLL_ECO_MS
                           : (synced ? DIMMER_STATE_POLL_SYNC_MS
                                     : DIMMER_STATE_POLL_CATCHUP_MS);

          if (!linkStateKnown || !previousLinkOk) {
            if (gDisplayValues.dimmerTriggerPercent >= 0) {
              Serial.printf("[DIMMER] LINK OK ACTUAL=%d%% CMD=%d%% TEMP=%s C MAX=%d C TRIGGER=%d%% RELEASE=%.1f C RSSI=%d\n",
                            gDisplayValues.dimmerReported,
                            gDisplayValues.dimmerCommandReported,
                            gDisplayValues.temperature.length() > 0
                                ? gDisplayValues.temperature.c_str()
                                : "--.-",
                            effectiveMaxTemp,
                            gDisplayValues.dimmerTriggerPercent,
                            releaseTemp,
                            gDisplayValues.dimmerRssi);
            }
            else {
              Serial.printf("[DIMMER] LINK OK ACTUAL=%d%% CMD=%d%% TEMP=%s C MAX=%d C RELEASE=%.1f C(fallback) RSSI=%d\n",
                            gDisplayValues.dimmerReported,
                            gDisplayValues.dimmerCommandReported,
                            gDisplayValues.temperature.length() > 0
                                ? gDisplayValues.temperature.c_str()
                                : "--.-",
                            effectiveMaxTemp,
                            releaseTemp,
                            gDisplayValues.dimmerRssi);
            }
          }
        }
        else {
          gDisplayValues.dimmerCommOk = false;
          errorReason = error ? String("JSON ") + error.c_str()
                              : "invalid /state JSON";
        }
      }
      else {
        gDisplayValues.dimmerCommOk = false;
        errorReason = "HTTP " + String(httpResponseCode);
      }

      httpdimmer.end();
    }
    else {
      gDisplayValues.dimmerCommOk = false;
      errorReason = "HTTP begin failed";
    }

    if (!currentLinkOk) {
      gDisplayValues.temperature = "";
    }

    if (!currentLinkOk && (!linkStateKnown || previousLinkOk)) {
      Serial.printf("[DIMMER] LINK ERROR (%s)\n", errorReason.c_str());
    }

    linkStateKnown = true;
    previousLinkOk = currentLinkOk;

#ifdef TTGO
    gDisplayForceRefresh = true;
#endif

    vTaskDelay(nextPollMs / portTICK_PERIOD_MS);
  }
}

#endif
