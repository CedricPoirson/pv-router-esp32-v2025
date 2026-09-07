#ifndef TASK_WIFI_CONNECTION
#define TASK_WIFI_CONNECTION

#include <Arduino.h>
#include "WiFi.h"
#include "../config/enums.h"
#include "../config/config.h"

extern DisplayValues gDisplayValues;
extern void goToDeepSleep();
extern Configwifi configwifi;

static bool useSpiffsWifiCredentials()
{
    // Keep the reconnect path aligned with setup(): when WIFI_PASSWORD is set
    // to "xxx", credentials come from /wifi.json in SPIFFS.
    return strcmp(WIFI_PASSWORD, "xxx") == 0;
}

/**
 * Task: monitor the WiFi connection and keep it alive.
 */
void keepWiFiAlive(void * parameter){
    for(;;){
        if(WiFi.status() == WL_CONNECTED){
            vTaskDelay(30000 / portTICK_PERIOD_MS);
            continue;
        }

        serial_println(F("[WIFI] Connecting"));
        gDisplayValues.currentState = CONNECTING_WIFI;

        WiFi.mode(WIFI_STA);
        WiFi.setHostname(DEVICE_NAME);
        if (useSpiffsWifiCredentials()) {
            WiFi.begin(configwifi.SID, configwifi.passwd);
        }
        else {
            WiFi.begin(WIFI_NETWORK, WIFI_PASSWORD);
        }

        unsigned long startAttemptTime = millis();

        while (WiFi.status() != WL_CONNECTED &&
                millis() - startAttemptTime < WIFI_TIMEOUT){}

        if(WiFi.status() != WL_CONNECTED){
            serial_println(F("[WIFI] FAILED"));
            vTaskDelay(WIFI_RECOVER_TIME_MS / portTICK_PERIOD_MS);
            continue;
        }

        serial_print(F("[WIFI] Connected: "));
        serial_println(WiFi.localIP());
        gDisplayValues.currentState = UP;
        gDisplayValues.IP = String(WiFi.localIP().toString());
    }
}

#endif
