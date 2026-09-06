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

// Fronius Inverter
#include <HTTPClient.h>
#include <ArduinoJson.h>

extern DisplayValues gDisplayValues;
extern Config config;

int Pow_mqtt_send = 0;

// Incremented only after a complete, validated PowerFlow sample has been
// written to gDisplayValues. The dimmer task uses this counter to react once
// per fresh Fronius sample instead of waiting on an unrelated 5 s timer.
volatile uint32_t gFroniusSampleCounter = 0;

void measureElectricityf(void * parameter)
{
    for (;;) {
#if WIFI_ACTIVE == true
        HTTPClient http;
        String url = "http://" + String(IP_FRONIUS) +
                     "/solar_api/v1/GetPowerFlowRealtimeData.fcgi";

        http.setTimeout(3000);
        http.begin(url);
        int httpCode = http.GET();

        Serial.printf("httpCode / function measure: %d\n", httpCode);

        bool validFroniusSample = false;

        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            DynamicJsonDocument doc(2048);
            DeserializationError error = deserializeJson(doc, payload);

            if (!error) {
                int apiStatus = doc["Head"]["Status"]["Code"] | -1;
                JsonVariant gridValue = doc["Body"]["Data"]["Site"]["P_Grid"];

                if (apiStatus == 0 && !gridValue.isNull()) {
                    gDisplayValues.grid = gridValue.as<double>();

                    // Prefer Site.P_PV from the same PowerFlow response.
                    // Fall back to inverter 1 power for older/variant responses.
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
                    Serial.printf("Fronius API invalid: Status.Code=%d P_Grid=%s\n",
                                  apiStatus,
                                  gridValue.isNull() ? "null" : "present");
                }
            }
            else {
                Serial.print("Fronius JSON error: ");
                Serial.println(error.c_str());
            }
        }

        http.end();

        gDisplayValues.froniusup = validFroniusSample;

        if (validFroniusSample) {
            // Publish the sample only after every value above is complete.
            gFroniusSampleCounter++;
            Serial.printf("[FRONIUS #%lu] PV=%.0f W GRID=%.0f W OK=1\n",
                          (unsigned long)gFroniusSampleCounter,
                          gDisplayValues.production,
                          gDisplayValues.grid);
        }
        else {
            Serial.println("[FRONIUS] sample invalid - control disabled for this cycle");
        }

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
