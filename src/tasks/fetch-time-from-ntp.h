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

    // Day of week for a Gregorian date: Sunday = 0 ... Saturday = 6.
    static int ntpDayOfWeek(int year, int month, int day)
    {
        static const int monthTable[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
        if (month < 3) year -= 1;
        return (year + year / 4 - year / 100 + year / 400 +
                monthTable[month - 1] + day) % 7;
    }

    static int ntpLastSundayOfMonth(int year, int month)
    {
        // March and October both have 31 days.
        return 31 - ntpDayOfWeek(year, month, 31);
    }

    // Europe/Paris daylight-saving rules:
    // CEST starts on the last Sunday of March at 01:00 UTC.
    // CEST ends   on the last Sunday of October at 01:00 UTC.
    static bool ntpParisSummerTime(time_t utcEpoch)
    {
        struct tm utcTime;
        gmtime_r(&utcEpoch, &utcTime);

        const int year = utcTime.tm_year + 1900;
        const int month = utcTime.tm_mon + 1;
        const int day = utcTime.tm_mday;
        const int hour = utcTime.tm_hour;

        if (month < 3 || month > 10) return false;
        if (month > 3 && month < 10) return true;

        if (month == 3) {
            const int lastSunday = ntpLastSundayOfMonth(year, 3);
            if (day > lastSunday) return true;
            if (day < lastSunday) return false;
            return hour >= 1;
        }

        // October
        const int lastSunday = ntpLastSundayOfMonth(year, 10);
        if (day < lastSunday) return true;
        if (day > lastSunday) return false;
        return hour < 1;
    }

    static void ntpApplyParisTimezone()
    {
        // NTPClient stores the configured offset inside getEpochTime().
        // Temporarily remove it so DST is always calculated from true UTC.
        timeClient.setTimeOffset(0);
        const time_t utcEpoch = (time_t)timeClient.getEpochTime();

        const bool summerTime = ntpParisSummerTime(utcEpoch);
        const long parisOffsetSeconds = summerTime ? 7200L : 3600L;
        timeClient.setTimeOffset(parisOffsetSeconds);

        Serial.printf("[NTP] Europe/Paris: %s UTC%+ldh -> %s\n",
                      summerTime ? "CEST" : "CET",
                      parisOffsetSeconds / 3600L,
                      timeClient.getFormattedTime().c_str());
    }

    void fetchTimeFromNTP(void * parameter){
        for(;;){
            if(!WiFi.isConnected()){
                // Retry shortly after a WiFi interruption.
                vTaskDelay(10 * 1000 / portTICK_PERIOD_MS);
                continue;
            }

            serial_println("[NTP] Updating...");

            // Calling update every minute does not generate an NTP request every
            // minute: NTPClient still honours NTP_UPDATE_INTERVAL_MS internally.
            // The short loop lets the DST transition be applied within ~1 minute.
            timeClient.setTimeOffset(0);
            timeClient.update();
            ntpApplyParisTimezone();

            serial_println("[NTP] Done");

            // Re-evaluate Europe/Paris DST every minute. Network NTP refreshes
            // remain limited by NTP_UPDATE_INTERVAL_MS (currently one hour).
            vTaskDelay(60 * 1000 / portTICK_PERIOD_MS);
        }
    }

#endif
#endif
