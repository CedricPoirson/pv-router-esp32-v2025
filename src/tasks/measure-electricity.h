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
#include <ArduinoJson.h>  // Make sure this is included for JSON handling

extern DisplayValues gDisplayValues;
extern Config config; 

int Pow_mqtt_send = 0;

void measureElectricityf(void * parameter)
{
    for(;;){
        long start = millis();

        #if WIFI_ACTIVE == true
            HTTPClient http;
            String url = "http://" + String(IP_FRONIUS) + "/solar_api/v1/GetPowerFlowRealtimeData.fcgi";
            String url2 = "http://" + String(IP_FRONIUS) + "/solar_api/v1/GetPowerFlowRealtimeData.fcgi";
            
            http.begin(url);
            int httpCode = http.GET();

            Serial.print("httpCode / function measure: ");
            Serial.println(httpCode);

            #if(httpCode == HTTP_CODE_OK) 
                String payload = http.getString();
                DynamicJsonDocument doc(900);
                DeserializationError error = deserializeJson(doc, payload);

                long generatedPower = doc["Body"]["Data"]["Inverters"]["1"]["P"];
                gDisplayValues.production  = generatedPower;
            #else
                gDisplayValues.froniusup = false;
                Serial.println("gDisplayValues.froniusup = false");
            #endif
            http.end();

            HTTPClient http2;
            http2.begin(url2);
            httpCode = http2.GET();
            #if(httpCode == HTTP_CODE_OK) 
                String payload2 = http2.getString();
                DynamicJsonDocument doc2(1500);
                error = deserializeJson(doc2, payload2);

                long generatedPower2 = doc2["Body"]["Data"]["Site"]["P_Grid"];
                gDisplayValues.watt  =  generatedPower2;
                gDisplayValues.froniusup = true;
            #else
                gDisplayValues.froniusup = false;
            #endif
            http2.end();
            Serial.print("generatedPower2 / function measure: ");
            Serial.println(generatedPower2);
            Serial.print("gDisplayValues.production / function measure: ");
            Serial.println(gDisplayValues.production);
            Serial.print("gDisplayValues.watt / function measure: ");
            Serial.println(gDisplayValues.watt);
            Serial.print("gDisplayValues.froniusup / function measure: ");
            Serial.println(gDisplayValues.froniusup);
        #endif

        long end = millis();

        #if WIFI_ACTIVE == true
            Pow_mqtt_send++;
            if (Pow_mqtt_send > 10) {
                Mqtt_send(String(config.IDX), String(int(gDisplayValues.watt)));  
                Pow_mqtt_send = 0;
            }
        #endif
        
        vTaskDelay(2000 / portTICK_PERIOD_MS);  // Delay for 2 seconds to avoid overloading the system
    }    
}

#endif
