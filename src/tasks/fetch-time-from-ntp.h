#ifndef TASK_FETCH_TIME_NTP
#define TASK_FETCH_TIME_NTP

#if NTP_TIME_SYNC_ENABLED == true
    #include <Arduino.h>
    #include <WiFi.h>
    #include <NTPClient.h>
    #include <WiFiUdp.h>
    #include <time.h>
    #include "../config/enums.h"

    extern void reconnectWifiIfNeeded();
    extern DisplayValues gDisplayValues;

    extern NTPClient timeClient;

    void fetchTimeFromNTP(void * parameter){
        bool timezoneConfigured = false;

        for(;;){
            
            if(!WiFi.isConnected()){
                vTaskDelay(10*1000 / portTICK_PERIOD_MS);
                continue;
            }

            serial_println("[NTP] Updating...");

            if(!timezoneConfigured) {
                // Europe/Paris timezone with automatic daylight saving time
                setenv("TZ", "Europe/Paris", 1);
                tzset();
                configTime(0, 0, "pool.ntp.org", "time.nist.gov");
                timezoneConfigured = true;
            }

            struct tm timeinfo;
            if(getLocalTime(&timeinfo, 10000)) {
                char buffer[32];
                strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
                gDisplayValues.time = String(buffer);
                serial_print("[NTP] Time: ");
                serial_println(buffer);
            } else {
                serial_println("[NTP] Failed to get time");
            }

            serial_println("[NTP] Done");
            
            vTaskDelay(NTP_UPDATE_INTERVAL_MS / portTICK_PERIOD_MS);
        }
    }

#endif
#endif
