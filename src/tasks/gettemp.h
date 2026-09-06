#ifndef TASK_GET_TEMP
#define TASK_GET_TEMP

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "../config/config.h"
#include "../config/enums.h"

#ifndef DIMMER_HTTP_TIMEOUT_MS
#define DIMMER_HTTP_TIMEOUT_MS 500
#endif

#ifndef DIMMER_STATE_REFRESH_MS
#define DIMMER_STATE_REFRESH_MS 3000UL
#endif

extern DisplayValues gDisplayValues;
extern Config config;

/**
 * Poll the RobotDyn firmware 20260514 state endpoint.
 *
 * GET http://<dimmer>/state returns JSON containing dimmer, commande,
 * temperature/dallas0, power, Ptotal, RSSI, onoff, alerte and version.
 * A state-polling failure affects telemetry only; it never blocks the Fronius
 * regulation task.
 */
void GetDImmerTemp(void * parameter) {
  (void)parameter;

  for (;;) {
    bool stateValid = false;

#if WIFI_ACTIVE == true
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient httpdimmer;
      httpdimmer.setConnectTimeout(DIMMER_HTTP_TIMEOUT_MS);
      httpdimmer.setTimeout(DIMMER_HTTP_TIMEOUT_MS);

      if (httpdimmer.begin(String(config.dimmer), 80, "/state")) {
        const int httpResponseCode = httpdimmer.GET();

        if (httpResponseCode >= 200 && httpResponseCode < 300) {
          const String payload = httpdimmer.getString();
          StaticJsonDocument<1024> doc;
          const DeserializationError error = deserializeJson(doc, payload);

          if (!error && !doc["dimmer"].isNull() && !doc["commande"].isNull()) {
            gDisplayValues.dimmerApplied = doc["dimmer"] | 0;
            gDisplayValues.dimmerCommand = doc["commande"] | 0;
            gDisplayValues.dimmerPower = doc["power"] | 0.0f;
            gDisplayValues.dimmerPtotal = doc["Ptotal"] | 0.0f;
            gDisplayValues.dimmerRssi = doc["RSSI"] | 0;
            gDisplayValues.dimmerOn = doc["onoff"] | false;

            gDisplayValues.dimmerAlert = doc["alerte"].as<String>();
            gDisplayValues.dimmerVersion = doc["version"].as<String>();
            gDisplayValues.dimmerAlarm =
                gDisplayValues.dimmerAlert.length() > 0 &&
                gDisplayValues.dimmerAlert != "RAS";

            String temperature = doc["dallas0"].as<String>();
            if (temperature.length() == 0) {
              temperature = doc["temperature"].as<String>();
            }
            if (temperature.length() > 0) {
              gDisplayValues.temperature = temperature;
            }

            stateValid = true;

#if DEBUG == true
            Serial.print(F("Dimmer /state: applied="));
            Serial.print(gDisplayValues.dimmerApplied);
            Serial.print(F("% command="));
            Serial.print(gDisplayValues.dimmerCommand);
            Serial.print(F("% temp="));
            Serial.print(gDisplayValues.temperature);
            Serial.print(F("C RSSI="));
            Serial.print(gDisplayValues.dimmerRssi);
            Serial.print(F(" alert="));
            Serial.println(gDisplayValues.dimmerAlert);
#endif
          } else {
            Serial.print(F("Invalid dimmer /state JSON: "));
            Serial.println(error.c_str());
          }
        } else {
          Serial.print(F("Dimmer /state HTTP error: "));
          Serial.println(httpResponseCode);
        }

        httpdimmer.end();
      }
    }
#endif

    gDisplayValues.dimmerOnline = stateValid;
    vTaskDelay(DIMMER_STATE_REFRESH_MS / portTICK_PERIOD_MS);
  }
}

#endif
