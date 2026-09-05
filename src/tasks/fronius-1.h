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





    HTTPClient http;

    #if WIFI_ACTIVE == true

            String url = "http://" + String(IP_FRONIUS) + "/solar_api/v1/GetInverterRealtimeData.cgi?Scope=System";
        
            http.begin(url);

            // start connection and send HTTP header
        #if(httpCode == HTTP_CODE_OK) 
                int httpCode = http.GET();
                String payload = http.getString();
                DynamicJsonDocument doc(900);
                DeserializationError error = deserializeJson(doc, payload);
                long generatedPower = doc["Body"]["Data"]["PAC"]["Values"]["1"];
                //String displayedPower = String(generatedPower) + " W";
                gDisplayValues.production  = generatedPower;
            http.end();
        #endif

/*
        String url2 = "http://" + String(IP_FRONIUS) + "/solar_api/v1/GetMeterRealtimeData.cgi?Scope=System";
            http.begin(url2); 
            
        #if(httpCode == HTTP_CODE_OK) 
                httpCode = http.GET();
                payload = http.getString();
                DynamicJsonDocument doc2(1500);
                error = deserializeJson(doc2, payload);
                long generatedPower2 = doc2["Body"]["Data"]["0"]["PowerReal_P_Sum"];
                //String displayedPower2 = String(generatedPower2) + " W";
                gDisplayValues.watt  = generatedPower2;
        http.end();
        #endif

        */
        
        String url2 = "http://" + String(IP_FRONIUS) + "/solar_api/v1/GetPowerFlowRealtimeData.fcgi";
http.begin(url2); 

int httpCode = http.GET();  // Effectuer la requête GET

if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();  // Récupérer la réponse sous forme de chaîne
    DynamicJsonDocument doc2(1500);  // Créer un document JSON pour désérialiser la réponse
    DeserializationError error = deserializeJson(doc2, payload);  // Désérialiser la chaîne JSON

    if (error) {
        Serial.println("Erreur de désérialisation JSON");
        return;  // Sortir si la désérialisation échoue
    }

    // Extraire la puissance consommée en utilisant la nouvelle structure JSON
    long consumedPower = doc2["Body"]["Data"]["Site"]["P_Load"];  // Récupérer la puissance consommée
    gDisplayValues.watt = consumedPower;  // Mettre à jour la variable globale avec la puissance consommée

    // Affichage pour vérifier la valeur
    Serial.print("Puissance consommée : ");
    Serial.println(gDisplayValues.watt);

    http.end();  // Terminer la connexion HTTP
    



} else {
    Serial.print("Erreur HTTP: ");
    Serial.println(httpCode);  // Afficher le code d'erreur si la requête échoue
    http.end();
}

        Serial.print("gDisplayValues.watt fct draw: ");
        Serial.println(gDisplayValues.watt);


    #endif




        
        long end = millis();
        
    #if WIFI_ACTIVE == true
        Pow_mqtt_send ++ ;
        if ( Pow_mqtt_send > 10 ) {
            Mqtt_send(String(config.IDX), String(int(gDisplayValues.watt)));  
            Pow_mqtt_send = 0 ;
        }
    #endif
        
  


      // Schedule the task to run again in 1 second (while
      // taking into account how long measurement took)
      vTaskDelay((1000-(end-start)) / portTICK_PERIOD_MS);
    }    
}

#endif

