#ifndef TASK_MEASURE_ELECTRICITY
#define TASK_MEASURE_ELECTRICITY

#include <Arduino.h>
//#include "EmonLib.h"

#include "config/config.h"
#include "config/enums.h"
#include "mqtt-aws.h"
#include "mqtt-home-assistant.h"
#include "functions/energyFunctions.h"
#include "functions/dimmerFunction.h"
#include "functions/drawFunctions.h"

// Fronius Inverter
#include "HTTPClient.h"
//const char *HOST = "192.168.100.245";


extern DisplayValues gDisplayValues;
//extern EnergyMonitor emon1;
extern Config config; 


int Pow_mqtt_send = 0;

void measureElectricityf(void * parameter)
{
    for(;;){
    //  serial_println("[ENERGY] Measuring...");
       /// vérification qu'une autre task ne va pas fausser les valeurs
      long start = millis();

  

    #if WIFI_ACTIVE == true
      
            HTTPClient http;
            String url = "http://" + String(IP_FRONIUS) + "/solar_api/v1/GetPowerFlowRealtimeData.fcgi";
            String url2 = "http://" + String(IP_FRONIUS) + "/solar_api/v1/GetPowerFlowRealtimeData.fcgi";
            http.begin(url);
            int httpCode = http.GET();
            // start connection and send HTTP header

            Serial.print(" httpcode/ fonction mesure: ");
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

                  
                  payload = http2.getString();
                  DynamicJsonDocument doc2(1500);
                  error = deserializeJson(doc2, payload);
                  long generatedPower2 = doc2["Body"]["Data"]["Site"]["P_Load"];
                  gDisplayValues.watt  = generatedPower2;
                  gDisplayValues.froniusup = true;
            
            #else
            
                  gDisplayValues.froniusup = false;
            
            
            #endif
            http2.end();
        
        
        Serial.print("gDisplayValues.production / fonction mesure: ");
        Serial.println(gDisplayValues.production);
        Serial.print("gDisplayValues.watt / fonction mesure: ");
        Serial.println(gDisplayValues.watt);
        Serial.print("gDisplayValues.froniusup  / fonction mesure: : ");
        Serial.println(gDisplayValues.froniusup);

    #endif




        
        long end = millis();
        
    #if WIFI_ACTIVE == true
        Pow_mqtt_send ++ ;
        if ( Pow_mqtt_send > 10 ) {
            Mqtt_send(String(config.IDX), String(int(gDisplayValues.watt)));  
            Pow_mqtt_send = 0 ;
        }
    #endif
        
  

        //Serial.print("2000-(end-start)) / portTICK_PERIOD_MS  / fonction mesure: : ");
        //Serial.println((2000-(end-start)) / portTICK_PERIOD_MS);
      // Schedule the task to run again in 1 second (while
      // taking into account how long measurement took)
      //vTaskDelay((2000-(end-start)) / portTICK_PERIOD_MS);
      vTaskDelay( 2000 / portTICK_PERIOD_MS);
    }    
}

#endif

