#ifndef TASK_FETCH_TIME_NTP
#define TASK_FETCH_TIME_NTP

#if NTP_TIME_SYNC_ENABLED == true

#include <Arduino.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <time.h>
#include "../config/enums.h"

extern NTPClient timeClient;

// Fuseau Europe/Paris avec bascule automatique heure d'été / heure d'hiver.
static const char *PARIS_TZ = "CET-1CEST,M3.5.0/2,M10.5.0/3";

void fetchTimeFromNTP(void *parameter) {
  bool timezoneConfigured = false;

  for (;;) {
    if (!WiFi.isConnected()) {
      vTaskDelay(10 * 1000 / portTICK_PERIOD_MS);
      continue;
    }

    serial_println("[NTP] Updating...");

    // Configure aussi l'horloge système ESP32. SNTP gère ensuite la resynchronisation.
    if (!timezoneConfigured) {
      configTzTime(PARIS_TZ, NTP_SERVER);
      timezoneConfigured = true;
    }

    // NTPClient est conservé pour ne pas modifier le reste de l'affichage.
    // On ajuste simplement son offset selon l'heure d'été/hiver réellement active.
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 5000)) {
      timeClient.setTimeOffset(timeinfo.tm_isdst > 0 ? 7200 : 3600);
    }

    timeClient.update();

    serial_println("[NTP] Done");

    vTaskDelay(NTP_UPDATE_INTERVAL_MS / portTICK_PERIOD_MS);
  }
}

#endif
#endif
