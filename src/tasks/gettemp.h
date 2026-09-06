#ifndef TASK_GET_TEMP
#define TASK_GET_TEMP

#include <Arduino.h>

#include "../config/config.h"
#include "../config/enums.h"
#include "../functions/Mqtt_http_Functions.h"

HTTPClient httpdimmer;

extern DisplayValues gDisplayValues;

#ifdef TTGO
extern volatile bool gDisplayForceRefresh;
#endif

// Fast catch-up while the remote dimmer does not yet match the command,
// then a lighter steady-state poll once both sides are synchronized.
#define DIMMER_STATE_POLL_SYNC_MS    5000UL
#define DIMMER_STATE_POLL_CATCHUP_MS 2000UL
#define DIMMER_STATE_HTTP_TIMEOUT_MS  1200UL

void GetDImmerTemp(void * parameter){
  for (;;){
    unsigned long nextPollMs = DIMMER_STATE_POLL_CATCHUP_MS;

    String baseurl = "/state";
    httpdimmer.begin(String(config.dimmer), 80, baseurl);
    httpdimmer.setTimeout(DIMMER_STATE_HTTP_TIMEOUT_MS);
    int httpResponseCode = httpdimmer.GET();

    String dimmerstate = "";

    if (httpResponseCode == HTTP_CODE_OK) {
      dimmerstate = httpdimmer.getString();

      // Expected payload example:
      // 50;36.94;16;5;The_Wifi;-4
      // POWER;temperature;...
      int firstSeparator = dimmerstate.indexOf(';');
      int secondSeparator = (firstSeparator >= 0)
                              ? dimmerstate.indexOf(';', firstSeparator + 1)
                              : -1;

      if (firstSeparator > 0) {
        gDisplayValues.dimmerReported =
            dimmerstate.substring(0, firstSeparator).toInt();

        if (secondSeparator > firstSeparator) {
          gDisplayValues.temperature =
              dimmerstate.substring(firstSeparator + 1, secondSeparator);
        }
        else {
          gDisplayValues.temperature =
              dimmerstate.substring(firstSeparator + 1);
        }

        gDisplayValues.dimmerCommOk = true;
        gDisplayValues.dimmerLastOkMs = millis();

        int commandedDimmer = gDisplayValues.dimmer;
        if (commandedDimmer < 0) commandedDimmer = 0;
        if (commandedDimmer > 100) commandedDimmer = 100;

        int reportedDimmer = gDisplayValues.dimmerReported;
        if (reportedDimmer < 0) reportedDimmer = 0;
        if (reportedDimmer > 100) reportedDimmer = 100;

        const bool synced = abs(commandedDimmer - reportedDimmer) <= 2;
        nextPollMs = synced ? DIMMER_STATE_POLL_SYNC_MS
                            : DIMMER_STATE_POLL_CATCHUP_MS;

        Serial.printf("[DIMMER STATE] CMD=%d POWER=%d TEMP=%s OK=1 POLL=%lu ms\n",
                      commandedDimmer,
                      reportedDimmer,
                      gDisplayValues.temperature.c_str(),
                      nextPollMs);
      }
      else {
        gDisplayValues.dimmerCommOk = false;
        Serial.println("[DIMMER STATE] invalid payload - retry 2 s");
      }
    }
    else {
      Serial.printf("[DIMMER STATE] HTTP error=%d - retry 2 s\n",
                    httpResponseCode);
      gDisplayValues.dimmerCommOk = false;
    }

    httpdimmer.end();

#ifdef TTGO
    // Make the TTGO repaint immediately after a fresh dimmer read instead of
    // waiting for the periodic screen refresh.
    gDisplayForceRefresh = true;
#endif

    vTaskDelay(nextPollMs / portTICK_PERIOD_MS);
  }
}

#endif
