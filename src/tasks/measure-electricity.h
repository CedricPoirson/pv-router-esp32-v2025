#ifndef TASK_MEASURE_ELECTRICITY
#define TASK_MEASURE_ELECTRICITY

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config/config.h"
#include "config/enums.h"
#include "functions/Mqtt_http_Functions.h"

extern DisplayValues gDisplayValues;
extern Config config;

int Pow_mqtt_send = 0;

void measureElectricityf(void *parameter) {
  for (;;) {
#if WIFI_ACTIVE == true
    if (WiFi.isConnected()) {
      HTTPClient http;
      const String url = "http://" + String(IP_FRONIUS) +
                         "/solar_api/v1/GetPowerFlowRealtimeData.fcgi";

      http.setConnectTimeout(1500);
      http.setTimeout(2000);
      http.begin(url);

      const int httpCode = http.GET();

      if (httpCode == HTTP_CODE_OK) {
        const String payload = http.getString();
        DynamicJsonDocument doc(4096);
        const DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
          JsonVariant pGrid = doc["Body"]["Data"]["Site"]["P_Grid"];

          if (!pGrid.isNull()) {
            // P_Grid > 0 : import réseau / P_Grid < 0 : injection.
            gDisplayValues.watt = pGrid.as<double>();

            // Additionne les puissances si plusieurs onduleurs Fronius sont présents.
            double production = 0.0;
            JsonObject inverters = doc["Body"]["Data"]["Inverters"].as<JsonObject>();
            for (JsonPair inverter : inverters) {
              production += inverter.value()["P"] | 0.0;
            }
            gDisplayValues.production = production;
            gDisplayValues.froniusup = true;
          } else {
            gDisplayValues.froniusup = false;
            serial_println("[FRONIUS] P_Grid missing in JSON response");
          }
        } else {
          gDisplayValues.froniusup = false;
          serial_print("[FRONIUS] JSON error: ");
          serial_println(error.c_str());
        }
      } else {
        gDisplayValues.froniusup = false;
        serial_print("[FRONIUS] HTTP error: ");
        serial_println(httpCode);
      }

      http.end();
    } else {
      gDisplayValues.froniusup = false;
    }

    // Publication MQTT environ toutes les 22 secondes avec une mesure toutes les 2 s.
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
