#ifndef TASK_VERSIONED_DISPLAY
#define TASK_VERSIONED_DISPLAY

#include "../config/version.h"

#ifdef TTGO

// Add the connected SSID after the Wi-Fi RSSI on the diagnostic page. The
// RSSI keeps the normal colour coding while the SSID uses a smaller neutral
// font so reasonably long network names still fit on the 240 px display.
static void drawVersionedDiagnosticWifi(bool forceRedraw)
{
  static bool cacheValid = false;
  static bool lastConnected = false;
  static int lastRssi = -999;
  static String lastSsid;

  const bool connected = WiFi.isConnected();
  const int rssi = connected ? WiFi.RSSI() : -127;
  const String ssid = connected ? WiFi.SSID() : String("OFFLINE");

  if (!forceRedraw && cacheValid &&
      connected == lastConnected &&
      rssi == lastRssi &&
      ssid == lastSsid) {
    return;
  }

  int wifiColor = TFT_RED;
  if (rssi >= -60) wifiColor = TFT_GREEN;
  else if (rssi >= -75) wifiColor = TFT_YELLOW;

  display.fillRect(78, 22, 162, 16, TFT_BLACK);

  display.setTextSize(1);
  display.setTextFont(2);
  display.setTextColor(wifiColor, TFT_BLACK);
  const String rssiText = String(rssi) + " dBm";
  display.setCursor(79, 22, 2);
  display.print(rssiText);

  int ssidX = 79 + display.textWidth(rssiText, 2) + 6;
  const int availableWidth = max(0, 239 - ssidX);

  display.setTextFont(1);
  display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);

  String shownSsid = ssid;
  while (shownSsid.length() > 0 &&
         display.textWidth(shownSsid, 1) > availableWidth) {
    shownSsid.remove(shownSsid.length() - 1);
  }

  if (shownSsid.length() < ssid.length() && shownSsid.length() > 2) {
    shownSsid.remove(shownSsid.length() - 2);
    shownSsid += "..";
  }

  display.setCursor(ssidX, 25, 1);
  display.print(shownSsid);

  cacheValid = true;
  lastConnected = connected;
  lastRssi = rssi;
  lastSsid = ssid;
}

// Versioned display scheduler. A forced refresh means "draw now", not "blank
// the whole TFT". Full-screen clears are reserved for real page transitions.
// This keeps the differential renderer effective even though gettemp.h asks
// for an immediate refresh after RobotDyn updates.
void updateDisplaySmoothV144(void * parameter)
{
  (void)parameter;
  unsigned long lastDrawMs = 0;
  uint8_t lastPage = 255;

  for (;;) {
    if (!gDisplayBootComplete) {
      vTaskDelay(100 / portTICK_PERIOD_MS);
      continue;
    }

    const unsigned long now = millis();
    if (gDisplayForceRefresh ||
        (unsigned long)(now - lastDrawMs) >= 2000UL) {
      gDisplayForceRefresh = false;
      const uint8_t page = gDisplayPage;
      const bool fullRedraw = (page != lastPage);

      if (fullRedraw) {
        display.fillScreen(TFT_BLACK);
        lastPage = page;
      }

      if (page == 0) {
        drawTTGOSmoothDashboard(fullRedraw);
      }
      else if (page == 1) {
        drawTTGOSmoothDiagnostic(fullRedraw);
        drawVersionedDiagnosticWifi(fullRedraw);

        // Replace the historical diagnostic title with the actual firmware
        // version. The regulation version remains separate.
        if (fullRedraw) {
          display.fillRect(0, 0, 240, 18, TFT_BLACK);
          drawCenteredTTGO(String("DIAG ") + PV_ROUTER_FIRMWARE_LABEL,
                           1, 2, TFT_CYAN);
          display.drawFastHLine(0, 18, 240, TFT_DARKGREY);
        }
      }
      else if (page == 2) {
        // Help is a static page: draw it once when entering the page and then
        // leave it untouched. Previously page 2 fell through to diagnostics,
        // which immediately overwrote the help screen and made it appear only
        // as a brief flash.
        if (fullRedraw)
          drawTTGOHelpPage();
      }

      lastDrawMs = now;
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

#endif  // TTGO

#endif  // TASK_VERSIONED_DISPLAY
