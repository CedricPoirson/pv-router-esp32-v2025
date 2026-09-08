#ifndef TASK_VERSIONED_DISPLAY
#define TASK_VERSIONED_DISPLAY

#include "../config/version.h"

#ifdef TTGO

// Add the connected SSID after the Wi-Fi RSSI on the diagnostic page.
// RSSI and SSID use the same font/size as the diagnostic "WiFi" label.
// Keep the last non-empty SSID while connected because WiFi.SSID() can
// transiently return an empty String during internal Wi-Fi housekeeping.
static void drawVersionedDiagnosticWifi(bool forceRedraw)
{
  static bool cacheValid = false;
  static bool lastConnected = false;
  static int lastRssi = -999;
  static String lastSsid;
  static String stableSsid;

  const bool connected = WiFi.isConnected();
  const int rssi = connected ? WiFi.RSSI() : -127;

  String ssid;
  if (connected) {
    const String liveSsid = WiFi.SSID();
    if (liveSsid.length() > 0)
      stableSsid = liveSsid;

    ssid = stableSsid.length() > 0 ? stableSsid : String("WiFi");
  }
  else {
    ssid = "OFFLINE";
  }

  if (!forceRedraw && cacheValid &&
      connected == lastConnected &&
      rssi == lastRssi &&
      ssid == lastSsid) {
    return;
  }

  int wifiColor = TFT_RED;
  if (rssi >= -60) wifiColor = TFT_GREEN;
  else if (rssi >= -75) wifiColor = TFT_YELLOW;

  // One owner for the complete value area: clear and repaint RSSI + SSID
  // together so one part of the row can never erase the other.
  display.fillRect(78, 22, 162, 16, TFT_BLACK);
  display.setTextSize(1);
  display.setTextFont(2);

  const String rssiText = String(rssi) + " dBm";
  display.setTextColor(wifiColor, TFT_BLACK);
  display.setCursor(79, 22, 2);
  display.print(rssiText);

  const int ssidX = 79 + display.textWidth(rssiText, 2) + 5;
  const int availableWidth = max(0, 239 - ssidX);

  String shownSsid = ssid;
  if (display.textWidth(shownSsid, 2) > availableWidth) {
    const String suffix = "..";
    const int suffixWidth = display.textWidth(suffix, 2);

    while (shownSsid.length() > 0 &&
           display.textWidth(shownSsid, 2) + suffixWidth > availableWidth) {
      shownSsid.remove(shownSsid.length() - 1);
    }

    if (shownSsid.length() > 0)
      shownSsid += suffix;
  }

  // Same visual weight as the "WiFi" label on the left.
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setCursor(ssidX, 22, 2);
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
        // leave it untouched.
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
