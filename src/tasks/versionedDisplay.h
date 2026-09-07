#ifndef TASK_VERSIONED_DISPLAY
#define TASK_VERSIONED_DISPLAY

#include "../config/version.h"

#ifdef TTGO

// V14.4 display scheduler. A forced refresh means "draw now", not "blank the
// whole TFT". Full-screen clears are reserved for real page transitions.
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
      else {
        drawTTGOSmoothDiagnostic(fullRedraw);

        // Replace the historical V13 diagnostic title with the actual
        // firmware version. The regulation version remains V14.3 and is shown
        // separately on boot/Web diagnostics.
        if (fullRedraw) {
          display.fillRect(0, 0, 240, 18, TFT_BLACK);
          drawCenteredTTGO(String("DIAG ") + PV_ROUTER_FIRMWARE_LABEL,
                           1, 2, TFT_CYAN);
          display.drawFastHLine(0, 18, 240, TFT_DARKGREY);
        }
      }

      lastDrawMs = now;
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

#endif  // TTGO

#endif  // TASK_VERSIONED_DISPLAY
