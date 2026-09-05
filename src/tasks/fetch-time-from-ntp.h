#ifndef TASK_FETCH_TIME_NTP
#define TASK_FETCH_TIME_NTP

#if NTP_TIME_SYNC_ENABLED == true
    #include <Arduino.h>
    #include <WiFi.h>
    #include <NTPClient.h>
    #include <WiFiUdp.h>
    #include "../config/enums.h"

    extern void reconnectWifiIfNeeded();
    extern DisplayValues gDisplayValues;

    extern NTPClient timeClient;

    void fetchTimeFromNTP(void * parameter){
        for(;;){
            
            if(!WiFi.isConnected()){
                vTaskDelay(10*1000 / portTICK_PERIOD_MS);
                continue;
            }

            serial_println("[NTP] Updating...");

            // Europe/Paris timezone with automatic daylight saving time
            configTime(3600, 3600, "pool.ntp.org", "time.nist.gov");

            struct tm timeinfo;
            if(getLocalTime(&timeinfo)) {
                char buffer[32];
                strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
                gDisplayValues.time = String(buffer);
            }

            serial_println("[NTP] Done");
            
            vTaskDelay(NTP_UPDATE_INTERVAL_MS / portTICK_PERIOD_MS);
        }
    }

#endif
#endif
