#ifndef TASK_MEASURE_ELECTRICITY
#define TASK_MEASURE_ELECTRICITY

#include <Arduino.h>
#include "config/config.h"
#include "config/enums.h"
#include "mqtt-aws.h"
#include "mqtt-home-assistant.h"
#include "functions/energyFunctions.h"
#include "functions/dimmerFunction.h"
#include "functions/drawFunctions.h"

#include <HTTPClient.h>
#include <ArduinoJson.h>

extern DisplayValues gDisplayValues;
extern Config config;

int Pow_mqtt_send = 0;

// Incremented only after a complete, validated PowerFlow sample has been
// written to gDisplayValues. The dimmer task reacts once per fresh sample.
volatile uint32_t gFroniusSampleCounter = 0;

void measureElectricityf(void * parameter)
{
    bool froniusStateKnown = false;
    bool previousFroniusOk = false;

    for (;;) {
#if WIFI_ACTIVE == true
        HTTPClient http;
        String url = "http://" + String(IP_FRONIUS) +
                     "/solar_api/v1/GetPowerFlowRealtimeData.fcgi";

        http.setTimeout(3000);
        http.begin(url);
        int httpCode = http.GET();

        bool validFroniusSample = false;
        String errorReason = "unknown";

        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            DynamicJsonDocument doc(2048);
            DeserializationError error = deserializeJson(doc, payload);

            if (!error) {
                int apiStatus = doc["Head"]["Status"]["Code"] | -1;
                JsonVariant gridValue = doc["Body"]["Data"]["Site"]["P_Grid"];

                if (apiStatus == 0 && !gridValue.isNull()) {
                    gDisplayValues.grid = gridValue.as<double>();

                    JsonVariant pvValue = doc["Body"]["Data"]["Site"]["P_PV"];
                    if (!pvValue.isNull()) {
                        gDisplayValues.production = pvValue.as<double>();
                    }
                    else {
                        JsonVariant inverterPower = doc["Body"]["Data"]["Inverters"]["1"]["P"];
                        if (!inverterPower.isNull())
                            gDisplayValues.production = inverterPower.as<double>();
                    }

                    validFroniusSample = true;
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

        gDisplayValues.froniusup = validFroniusSample;

        if (validFroniusSample) {
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

        Pow_mqtt_send++;
        if (Pow_mqtt_send > 10) {
            Mqtt_send(String(config.IDX), String(int(gDisplayValues.grid)));
            Pow_mqtt_send = 0;
        }
#endif

        // Fronius Solar API realtime calls: one PowerFlow request every ~4 s.
        // P_Grid and P_PV are read from the same response.
        vTaskDelay(4000 / portTICK_PERIOD_MS);
    }
}

#endif
