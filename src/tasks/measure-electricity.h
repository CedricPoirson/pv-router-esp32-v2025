#ifndef TASK_MEASURE_ELECTRICITY
#define TASK_MEASURE_ELECTRICITY

#include <Arduino.h>
#include "config/config.h"
#include "config/enums.h"
#include "functions/energyFunctions.h"
#include "functions/dimmerFunction.h"
#include "functions/drawFunctions.h"
#include "functions/Mqtt_http_Functions.h"

#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <math.h>

#define FRONIUS_HTTP_TIMEOUT_MS 700UL
#define FRONIUS_POLL_INTERVAL_MS 1500UL

extern DisplayValues gDisplayValues;
extern Config config;

// Incremented only after a complete, validated PowerFlow sample has been
// written to gDisplayValues. The dimmer task reacts once per fresh sample.
volatile uint32_t gFroniusSampleCounter = 0;

void measureElectricityf(void * parameter)
{
    (void)parameter;

    bool froniusStateKnown = false;
    bool previousFroniusOk = false;

    for (;;) {
        const unsigned long cycleStartMs = millis();

#if WIFI_ACTIVE == true
        HTTPClient http;
        String url = "http://" + String(IP_FRONIUS) +
                     "/solar_api/v1/GetPowerFlowRealtimeData.fcgi";

        http.setConnectTimeout(FRONIUS_HTTP_TIMEOUT_MS);
        http.setTimeout(FRONIUS_HTTP_TIMEOUT_MS);

        bool validFroniusSample = false;
        String errorReason = "unknown";

        if (http.begin(url)) {
            const int httpCode = http.GET();

            if (httpCode == HTTP_CODE_OK) {
                const String payload = http.getString();
                StaticJsonDocument<2048> doc;
                const DeserializationError error = deserializeJson(doc, payload);

                if (!error) {
                    const int apiStatus = doc["Head"]["Status"]["Code"] | -1;
                    JsonVariant gridValue = doc["Body"]["Data"]["Site"]["P_Grid"];

                    if (apiStatus == 0 && !gridValue.isNull()) {
                        const double grid = gridValue.as<double>();

                        if (isfinite(grid) && fabs(grid) < 50000.0) {
                            gDisplayValues.grid = grid;

                            JsonVariant pvValue = doc["Body"]["Data"]["Site"]["P_PV"];
                            if (!pvValue.isNull()) {
                                const double pv = pvValue.as<double>();
                                if (isfinite(pv))
                                    gDisplayValues.production = pv;
                            }
                            else {
                                JsonVariant inverterPower =
                                    doc["Body"]["Data"]["Inverters"]["1"]["P"];
                                if (!inverterPower.isNull()) {
                                    const double pv = inverterPower.as<double>();
                                    if (isfinite(pv))
                                        gDisplayValues.production = pv;
                                }
                            }

                            validFroniusSample = true;
                        }
                        else {
                            errorReason = "invalid P_Grid";
                        }
                    }
                    else {
                        errorReason = "API Status.Code=" + String(apiStatus);
                        if (gridValue.isNull())
                            errorReason += " P_Grid=null";
                    }
                }
                else {
                    errorReason = "JSON ";
                    errorReason += error.c_str();
                }
            }
            else {
                errorReason = "HTTP " + String(httpCode);
            }

            http.end();
        }
        else {
            errorReason = "HTTP begin failed";
        }

        gDisplayValues.froniusup = validFroniusSample;

        if (validFroniusSample) {
            gDisplayValues.froniusLastOkMs = millis();
            gFroniusSampleCounter++;

            if (!froniusStateKnown || !previousFroniusOk) {
                Serial.printf("[FRONIUS] ONLINE PV=%.0f W GRID=%.0f W\n",
                              gDisplayValues.production,
                              gDisplayValues.grid);
            }
        }
        else if (!froniusStateKnown || previousFroniusOk) {
            Serial.printf("[FRONIUS] OFFLINE (%s)\n", errorReason.c_str());
        }

        froniusStateKnown = true;
        previousFroniusOk = validFroniusSample;

#if MQTT_CLIENT == true
        // Telemetry only. MQTT is not part of the regulation loop.
        Mqtt_publishState();
#endif
#endif

        const unsigned long elapsedMs = millis() - cycleStartMs;
        const unsigned long waitMs =
            (elapsedMs < FRONIUS_POLL_INTERVAL_MS)
                ? (FRONIUS_POLL_INTERVAL_MS - elapsedMs)
                : 1UL;

        vTaskDelay(waitMs / portTICK_PERIOD_MS);
    }
}

#endif
