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
#include "HTTPClient.h"
#include <ArduinoJson.h>

extern DisplayValues gDisplayValues;
extern Config config;

int Pow_mqtt_send = 0;

void measureElectricityf(void * parameter)
{
    for(;;){

        #if WIFI_ACTIVE == true
            HTTPClient http;
            String url = "http://" + String(IP_FRONIUS) + "/solar_api/v1/GetPowerFlowRealtimeData.fcgi";

            gDisplayValues.froniusup = false;

            http.begin(url);
            int httpCode = http.GET();

            Serial.print("httpCode / function measure: ");
            Serial.println(httpCode);

            if (httpCode == HTTP_CODE_OK) {
                String payload = http.getString();
                DynamicJsonDocument doc(2500);
                DeserializationError error = deserializeJson(doc, payload);

                if (!error && doc["Head"]["Status"]["Code"] == 0 && doc["Body"]["Data"].containsKey("Site")) {

                    JsonObject site = doc["Body"]["Data"]["Site"];

                    // Fronius production power
                    long generatedPower = 0;
                    if (!site["P_PV"].isNull()) {
                        generatedPower = site["P_PV"];
                    } else if (doc["Body"]["Data"]["Inverters"].containsKey("1")) {
                        generatedPower = doc["Body"]["Data"]["Inverters"]["1"]["P"];
                    }
                    gDisplayValues.production = generatedPower;

                    // House consumption (positive value)
                    long loadPower = site["P_Load"] | 0;
                    gDisplayValues.watt = abs(loadPower);

                    gDisplayValues.froniusup = true;

                } else {
                    Serial.println("Fronius JSON error or invalid status");
                    gDisplayValues.production = 0;
                }
            } else {
                Serial.println("Fronius API unavailable");
                gDisplayValues.production = 0;
            }

            http.end();

            Serial.print("gDisplayValues.production / function measure: ");
            Serial.println(gDisplayValues.production);
            Serial.print("gDisplayValues.watt / function measure: ");
            Serial.println(gDisplayValues.watt);
            Serial.print("gDisplayValues.froniusup / function measure: ");
            Serial.println(gDisplayValues.froniusup);
        #endif

        #if WIFI_ACTIVE == true
            Pow_mqtt_send++;
            if (Pow_mqtt_send > 10) {
                Mqtt_send(String(config.IDX), String(int(gDisplayValues.watt)));
                Pow_mqtt_send = 0;
            }
        #endif

        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

#endif
