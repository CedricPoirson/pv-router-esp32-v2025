#ifndef TASK_GET_TEMP
#define TASK_GET_TEMP

#include <Arduino.h>

#include "../config/config.h"
#include "../config/enums.h"
#include "../functions/Mqtt_http_Functions.h"

HTTPClient httpdimmer;

extern DisplayValues gDisplayValues;

void GetDImmerTemp(void * parameter){
  for (;;){
    String baseurl = "/state";
    httpdimmer.begin(String(config.dimmer), 80, baseurl);
    int httpResponseCode = httpdimmer.GET();

    String dimmerstate = "";

    if (httpResponseCode == HTTP_CODE_OK) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
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

        Serial.printf("[DIMMER STATE] POWER=%d TEMP=%s OK=1\n",
                      gDisplayValues.dimmerReported,
                      gDisplayValues.temperature.c_str());
      }
      else {
        gDisplayValues.dimmerCommOk = false;
        Serial.println("[DIMMER STATE] invalid payload");
      }
    }
    else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
      gDisplayValues.dimmerCommOk = false;
    }

    httpdimmer.end();

    // Refresh every GETTEMPREFRESH seconds.
    vTaskDelay(GETTEMPREFRESH * 1000 / portTICK_PERIOD_MS);
  }
}

#endif
