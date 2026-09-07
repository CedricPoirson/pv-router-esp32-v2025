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

#define DIMMER_STATE_POLL_SYNC_MS      5000UL
#define DIMMER_STATE_POLL_CATCHUP_MS   2000UL
#define DIMMER_STATE_HTTP_TIMEOUT_MS    500UL
#define DIMMER_CONFIG_POLL_MS         300000UL
#define DIMMER_CONFIG_RETRY_MS         10000UL

void GetDImmerTemp(void * parameter){
  (void)parameter;

  bool linkStateKnown = false;
  bool previousLinkOk = false;
  bool configKnown = false;
  unsigned long lastConfigAttemptMs = 0;

  for (;;) {
    const unsigned long now = millis();
    unsigned long nextPollMs = DIMMER_STATE_POLL_CATCHUP_MS;
    bool currentLinkOk = false;
    String errorReason = "unknown";

    // /config contains the real normal ECS maximum temperature (maxtemp).
    // Poll it once at startup, retry reasonably fast until it succeeds, then
    // refresh only every 5 minutes because this value changes very rarely.
    const unsigned long configInterval =
        configKnown ? DIMMER_CONFIG_POLL_MS : DIMMER_CONFIG_RETRY_MS;

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

            if (remoteMaxTemp > 0 && remoteMaxTemp <= 100) {
              const bool changed =
                  gDisplayValues.dimmerMaxTemp != remoteMaxTemp;
              const bool firstValidConfig = !configKnown;

              gDisplayValues.dimmerMaxTemp = remoteMaxTemp;
              configKnown = true;

              if (firstValidConfig || changed) {
                Serial.printf("[DIMMER] CONFIG OK MAX=%d C\n",
                              gDisplayValues.dimmerMaxTemp);
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

          const float waterTemp = gDisplayValues.temperature.toFloat();
          const int effectiveMaxTemp =
              gDisplayValues.dimmerMaxTemp > 0
                  ? gDisplayValues.dimmerMaxTemp
                  : config.tmax;
          const bool tempAtOrAboveMax =
              waterTemp > 0.0f &&
              effectiveMaxTemp > 0 &&
              waterTemp >= (float)effectiveMaxTemp;

          // Reuse the existing dimmerAlarm fail-safe path. A remote RobotDyn
          // alarm OR a locally detected ECS over-temperature forces POWER=0
          // from the Dimmer watchdog task.
          gDisplayValues.dimmerAlarm = remoteAlarm || tempAtOrAboveMax;

          gDisplayValues.dimmerCommOk = true;
          gDisplayValues.dimmerLastOkMs = millis();
          currentLinkOk = true;

          int commandedDimmer = constrain(gDisplayValues.dimmer, 0, 100);
          const bool synced =
              abs(commandedDimmer - gDisplayValues.dimmerReported) <= 2;
          nextPollMs = synced ? DIMMER_STATE_POLL_SYNC_MS
                              : DIMMER_STATE_POLL_CATCHUP_MS;

          if (!linkStateKnown || !previousLinkOk) {
            Serial.printf("[DIMMER] LINK OK ACTUAL=%d%% CMD=%d%% TEMP=%s C MAX=%d C RSSI=%d\n",
                          gDisplayValues.dimmerReported,
                          gDisplayValues.dimmerCommandReported,
                          gDisplayValues.temperature.length() > 0
                              ? gDisplayValues.temperature.c_str()
                              : "--.-",
                          effectiveMaxTemp,
                          gDisplayValues.dimmerRssi);
          }

          if (tempAtOrAboveMax) {
            Serial.printf("[DIMMER] ECS MAX TEMP %.1f/%d C -> POWER=0\n",
                          waterTemp,
                          effectiveMaxTemp);
          }
          else if (remoteAlarm) {
            Serial.printf("[DIMMER] ALARM: %s\n",
                          gDisplayValues.dimmerAlert.c_str());
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
      // Never keep displaying a stale live temperature. Keep the last valid
      // maxtemp from /config: it is configuration data, not a live reading.
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
