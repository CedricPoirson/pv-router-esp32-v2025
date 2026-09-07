#ifndef TASK_GET_TEMP
#define TASK_GET_TEMP

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "../config/config.h"
#include "../config/enums.h"
#include "../functions/Mqtt_http_Functions.h"

extern DisplayValues gDisplayValues;

#ifdef TTGO
extern volatile bool gDisplayForceRefresh;
#endif

#define DIMMER_STATE_POLL_SYNC_MS    5000UL
#define DIMMER_STATE_POLL_CATCHUP_MS 2000UL
#define DIMMER_STATE_HTTP_TIMEOUT_MS  500UL

void GetDImmerTemp(void * parameter){
  (void)parameter;

  bool linkStateKnown = false;
  bool previousLinkOk = false;

  for (;;) {
    unsigned long nextPollMs = DIMMER_STATE_POLL_CATCHUP_MS;
    bool currentLinkOk = false;
    String errorReason = "unknown";

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
          gDisplayValues.dimmerAlarm =
              gDisplayValues.dimmerAlert.length() > 0 &&
              !gDisplayValues.dimmerAlert.equalsIgnoreCase("RAS");

          gDisplayValues.dimmerCommOk = true;
          gDisplayValues.dimmerLastOkMs = millis();
          currentLinkOk = true;

          int commandedDimmer = constrain(gDisplayValues.dimmer, 0, 100);
          const bool synced =
              abs(commandedDimmer - gDisplayValues.dimmerReported) <= 2;
          nextPollMs = synced ? DIMMER_STATE_POLL_SYNC_MS
                              : DIMMER_STATE_POLL_CATCHUP_MS;

          if (!linkStateKnown || !previousLinkOk) {
            Serial.printf("[DIMMER] LINK OK ACTUAL=%d%% CMD=%d%% TEMP=%s C RSSI=%d\n",
                          gDisplayValues.dimmerReported,
                          gDisplayValues.dimmerCommandReported,
                          gDisplayValues.temperature.length() > 0
                              ? gDisplayValues.temperature.c_str()
                              : "--.-",
                          gDisplayValues.dimmerRssi);
          }

          if (gDisplayValues.dimmerAlarm) {
            Serial.printf("[DIMMER] ALARM: %s\n", gDisplayValues.dimmerAlert.c_str());
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
