#ifndef TASK_MEASURE_ELECTRICITY
#define TASK_MEASURE_ELECTRICITY

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <math.h>

#include "config/config.h"
#include "config/enums.h"
#include "mqtt-aws.h"
#include "mqtt-home-assistant.h"
#include "functions/energyFunctions.h"
#include "functions/dimmerFunction.h"
#include "functions/drawFunctions.h"

#ifndef FRONIUS_HTTP_TIMEOUT_MS
#define FRONIUS_HTTP_TIMEOUT_MS 700
#endif

#ifndef FRONIUS_REFRESH_MS
#define FRONIUS_REFRESH_MS 1500UL
#endif

extern DisplayValues gDisplayValues;
extern Config config;

int Pow_mqtt_send = 0;

/**
 * Read the Fronius locally. Regulation depends only on this local HTTP data;
 * MQTT/Home Assistant are optional telemetry.
 *
 * A sample is considered valid only when the HTTP request succeeds, the JSON
 * parses correctly, and Site/P_Grid exists and is finite. Any failure marks
 * Fronius unavailable immediately; the dimmer task then sends POWER=0.
 */
void measureElectricityf(void * parameter) {
  (void)parameter;

  for (;;) {
    const uint32_t start = millis();
    bool validSample = false;

#if WIFI_ACTIVE == true
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.setConnectTimeout(FRONIUS_HTTP_TIMEOUT_MS);
      http.setTimeout(FRONIUS_HTTP_TIMEOUT_MS);

      const String url =
          "http://" + String(IP_FRONIUS) +
          "/solar_api/v1/GetPowerFlowRealtimeData.fcgi";

      if (http.begin(url)) {
        const int httpCode = http.GET();

        if (httpCode == HTTP_CODE_OK) {
          const String payload = http.getString();
          StaticJsonDocument<2048> doc;
          const DeserializationError error = deserializeJson(doc, payload);

          JsonVariant pGridNode = doc["Body"]["Data"]["Site"]["P_Grid"];

          if (!error && !pGridNode.isNull()) {
            const float pGrid = pGridNode.as<float>();

            // Reject clearly impossible values as a corrupted measurement.
            if (isfinite(pGrid) && fabsf(pGrid) < 50000.0f) {
              gDisplayValues.watt = pGrid;

              JsonVariant pPvNode = doc["Body"]["Data"]["Site"]["P_PV"];
              if (!pPvNode.isNull()) {
                gDisplayValues.production = pPvNode.as<float>();
              } else {
                JsonVariant inverterNode = doc["Body"]["Data"]["Inverters"]["1"]["P"];
                gDisplayValues.production =
                    inverterNode.isNull() ? 0.0 : inverterNode.as<float>();
              }

              gDisplayValues.froniusLastOkMs = millis();
              gDisplayValues.froniusup = true;
              validSample = true;

#if DEBUG == true
              Serial.print(F("Fronius P_Grid: "));
              Serial.print(gDisplayValues.watt);
              Serial.print(F(" W | PV: "));
              Serial.print(gDisplayValues.production);
              Serial.println(F(" W"));
#endif
            } else {
              Serial.println(F("Fronius invalid P_Grid value"));
            }
          } else {
            Serial.print(F("Fronius JSON invalid/missing P_Grid: "));
            Serial.println(error.c_str());
          }
        } else {
          Serial.print(F("Fronius HTTP error: "));
          Serial.println(httpCode);
        }

        http.end();
      }
    }
#endif

    if (!validSample) {
      // The dimmer task observes this flag every 250 ms and immediately asks
      // the remote dimmer for POWER=0.
      gDisplayValues.froniusup = false;
    }

#if WIFI_ACTIVE == true && MQTT_CLIENT == true
    // Telemetry only: never part of the regulation path.
    Pow_mqtt_send++;
    if (Pow_mqtt_send > 10) {
      Mqtt_send(String(config.IDX), String((int)gDisplayValues.watt));
      Pow_mqtt_send = 0;
    }
#endif

    const uint32_t elapsed = millis() - start;
    const uint32_t waitMs =
        (elapsed < FRONIUS_REFRESH_MS) ? (FRONIUS_REFRESH_MS - elapsed) : 1;
    vTaskDelay(waitMs / portTICK_PERIOD_MS);
  }
}

#endif
