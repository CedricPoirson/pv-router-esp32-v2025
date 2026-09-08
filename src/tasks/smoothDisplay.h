#ifndef TASK_SMOOTH_DISPLAY
#define TASK_SMOOTH_DISPLAY

#ifdef TTGO

#include "displayHelp.h"

// Differential renderer for the TTGO ST7789. The standard display task used
// broad black clears before repainting dynamic areas; those blank intervals
// were visible as local flicker. This renderer updates only fields whose
// visible value/state changed and never blanks a full row during the 2 s loop.

struct SmoothDashboardCache {
  bool valid = false;
  bool wifiUp = false;
  String clockText;
  int tempTenths = 0;
  int maxTemp = 0;
  int tempColor = 0;
  int ceState = -1;
  int bannerMode = -1;
  int bannerBg = 0;
  int bannerWatts = 0;
  int pvWatts = 0;
  int gridWatts = 0;
  int gridMode = -1;
  int dimmerPercent = 0;
  int heaterWatts = 0;
  int ceRightMode = -1;
  int releaseTenths = 0;
  int houseWatts = 0;
};

static SmoothDashboardCache gSmoothDashboardCache;

static int tenthsTTGO(float value)
{
  return (int)(value * 10.0f + (value >= 0.0f ? 0.5f : -0.5f));
}

static void drawSmoothGaugeTTGO(int watts, bool valid, bool fullRedraw)
{
  static bool cacheValid = false;
  static bool lastValid = false;
  static int lastMarkerX = -1000;

  const int x = 2;
  const int y = 116;
  const int width = 236;
  const int height = 9;
  const int newMarkerX = valid ? gaugeXForPowerTTGO(watts) : -1000;

  if (!fullRedraw && cacheValid && lastValid == valid &&
      lastMarkerX == newMarkerX) {
    return;
  }

  // Erase only the previous cursor head. The coloured gauge itself is then
  // repainted directly with its final colours, so it never flashes black.
  if (!fullRedraw && cacheValid && lastValid && lastMarkerX >= 0) {
    const int clearX = max(0, lastMarkerX - 8);
    const int clearW = min(240 - clearX, 17);
    display.fillRect(clearX, y - 10, clearW, 10, TFT_BLACK);
  }

  const int xMinus500 = gaugeXForPowerTTGO(-500);
  const int xZero = gaugeXForPowerTTGO(0);
  const int xOneKw = gaugeXForPowerTTGO(1000);
  const int xThreeKw = gaugeXForPowerTTGO(3000);
  const int xMax = x + width - 1;

  display.fillRect(x, y, xMinus500 - x, height, TFT_RED);
  display.fillRect(xMinus500, y, xZero - xMinus500, height, TFT_ORANGE);
  display.fillRect(xZero, y, xOneKw - xZero, height, TFT_YELLOW);
  display.fillRect(xOneKw, y, xThreeKw - xOneKw, height, TFT_GREEN);
  display.fillRect(xThreeKw, y, xMax - xThreeKw + 1, height, TFT_CYAN);
  display.drawRect(x, y, width, height, TFT_WHITE);
  display.fillRect(xZero - 1, y - 2, 2, height + 4, TFT_LIGHTGREY);

  if (valid) {
    display.fillTriangle(newMarkerX - 7, y - 10,
                         newMarkerX + 7, y - 10,
                         newMarkerX, y - 1,
                         TFT_WHITE);
    display.fillRect(newMarkerX - 1, y - 1, 3, height + 2, TFT_WHITE);
  }

  // Scale labels are static; draw them only after a complete page clear.
  if (fullRedraw || !cacheValid) {
    display.setTextFont(1);
    display.setTextSize(1);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    const int labelY = 127;
    const int values[5] = {-2000, 0, 2000, 4000, 6000};
    const char *labels[5] = {"-2", "0", "2", "4", "6"};
    for (int i = 0; i < 5; i++) {
      const int tickX = gaugeXForPowerTTGO(values[i]);
      const int textWidth = display.textWidth(labels[i], 1);
      int textX = tickX - textWidth / 2;
      if (textX < 0) textX = 0;
      if (textX + textWidth > 240) textX = 240 - textWidth;
      display.setCursor(textX, labelY, 1);
      display.print(labels[i]);
    }
    display.setCursor(219, labelY, 1);
    display.print("kW");
  }

  cacheValid = true;
  lastValid = valid;
  lastMarkerX = newMarkerX;
}

static void drawSmoothBannerTTGO(int mode,
                                 int bg,
                                 int fg,
                                 int watts,
                                 bool fullRedraw)
{
  const int bannerY = 20;
  const int bannerH = 47;
  const bool cacheValid = gSmoothDashboardCache.valid;

  String label;
  switch (mode) {
    case 1: label = "IMPORT"; break;
    case 2: label = "ZERO GRID"; break;
    case 3: label = "DISPO"; break;
    case 4: label = "SURPLUS"; break;
    default: label = "FRONIUS"; break;
  }

  const String value = mode == 0 ? String("---") : formatPowerTTGO(watts);
  const bool styleChanged =
      fullRedraw || !cacheValid ||
      gSmoothDashboardCache.bannerMode != mode ||
      gSmoothDashboardCache.bannerBg != bg;

  if (styleChanged) {
    display.fillRect(0, bannerY, 240, bannerH, bg);

    const bool largeLabel = (mode == 3 || mode == 4);
    const int labelFont = largeLabel ? 1 : 2;
    const int labelSize = largeLabel ? 2 : 1;
    const int labelY = largeLabel ? bannerY + 1 : bannerY + 2;
    display.setTextSize(labelSize);
    display.setTextFont(labelFont);
    display.setTextColor(fg, bg);
    int labelX = (240 - display.textWidth(label, labelFont)) / 2;
    if (labelX < 0) labelX = 0;
    display.setCursor(labelX, labelY, labelFont);
    display.print(label);

    display.setTextSize(1);
    display.setTextFont(4);
    display.setTextColor(fg, bg);
    const int valueWidth = display.textWidth(value, 4) + 1;
    int valueX = (240 - valueWidth) / 2;
    if (valueX < 0) valueX = 0;
    const int valueY = bannerY + 18;
    display.setCursor(valueX, valueY, 4);
    display.print(value);
    display.setTextColor(fg);
    display.setCursor(valueX + 1, valueY, 4);
    display.print(value);
  }
  else if (gSmoothDashboardCache.bannerWatts != watts) {
    // Same banner colour/label: erase only the old numeric glyph footprint.
    // Because the erase colour equals the final background, no flash is seen.
    display.setTextSize(1);
    display.setTextFont(4);
    const String oldValue =
        gSmoothDashboardCache.bannerMode == 0
            ? String("---")
            : formatPowerTTGO(gSmoothDashboardCache.bannerWatts);
    const int oldW = display.textWidth(oldValue, 4) + 4;
    const int newW = display.textWidth(value, 4) + 4;
    const int clearW = max(oldW, newW);
    const int clearX = max(0, (240 - clearW) / 2);
    display.fillRect(clearX, bannerY + 17, min(clearW, 240 - clearX), 30, bg);

    const int valueWidth = display.textWidth(value, 4) + 1;
    int valueX = (240 - valueWidth) / 2;
    if (valueX < 0) valueX = 0;
    display.setTextColor(fg, bg);
    display.setCursor(valueX, bannerY + 18, 4);
    display.print(value);
    display.setTextColor(fg);
    display.setCursor(valueX + 1, bannerY + 18, 4);
    display.print(value);
  }

  gSmoothDashboardCache.bannerMode = mode;
  gSmoothDashboardCache.bannerBg = bg;
  gSmoothDashboardCache.bannerWatts = watts;
}

static void drawTTGOSmoothDashboard(bool fullRedraw)
{
  bool wifiUp = (gDisplayValues.currentState == UP);

  if (!wifiUp) {
    if (fullRedraw || !gSmoothDashboardCache.valid ||
        gSmoothDashboardCache.wifiUp) {
      display.fillScreen(TFT_BLACK);
      drawCenteredTTGO("NO WIFI", 48, 4, TFT_RED);
    }
    gSmoothDashboardCache.valid = true;
    gSmoothDashboardCache.wifiUp = false;
    return;
  }

  if (!gSmoothDashboardCache.valid || !gSmoothDashboardCache.wifiUp) {
    if (!fullRedraw) display.fillScreen(TFT_BLACK);
    fullRedraw = true;
  }
  gSmoothDashboardCache.wifiUp = true;

  const unsigned long now = millis();
  const bool dimmerFresh =
      gDisplayValues.dimmerCommOk &&
      gDisplayValues.dimmerLastOkMs > 0 &&
      ((unsigned long)(now - gDisplayValues.dimmerLastOkMs) <= 15000UL);

  int reportedDimmer = dimmerFresh ? gDisplayValues.dimmerReported : 0;
  reportedDimmer = constrain(reportedDimmer, 0, 100);
  int commandedDimmer = constrain(gDisplayValues.dimmer, 0, 100);

  const int configuredHeaterPowerW =
      constrain(config.heaterPowerW > 0 ? config.heaterPowerW : 800, 100, 5000);
  const int heaterPower = (configuredHeaterPowerW * reportedDimmer) / 100;
  const int grid = (int)gDisplayValues.grid;
  const int pvPower = (int)gDisplayValues.production;
  const float waterTemp = gDisplayValues.temperature.toFloat();
  const int effectiveMaxTemp =
      gDisplayValues.dimmerMaxTemp > 0 ? gDisplayValues.dimmerMaxTemp : config.tmax;

  int housePower = pvPower + grid;
  if (housePower < 0) housePower = 0;

  const bool tempAtOrAboveMax =
      waterTemp > 0.0f && effectiveMaxTemp > 0 &&
      waterTemp >= (float)effectiveMaxTemp;
  const bool heaterTempHold =
      dimmerFresh && gDisplayValues.dimmerTempLimitActive;
  const bool heaterAtTempLimit = heaterTempHold && tempAtOrAboveMax;
  const bool dimmerSynced =
      dimmerFresh && (abs(commandedDimmer - reportedDimmer) <= 2);

  if (dimmerFresh && commandedDimmer > 0 && reportedDimmer < 5 &&
      !heaterTempHold) {
    if (gDimmerStartSinceMs == 0) gDimmerStartSinceMs = now;
  }
  else {
    gDimmerStartSinceMs = 0;
  }
  const bool dimmerStarting =
      gDimmerStartSinceMs > 0 &&
      ((unsigned long)(now - gDimmerStartSinceMs) <= 15000UL);

  const int gridDisplayNeutralW = 20;
  const bool importing = gDisplayValues.froniusup && grid > gridDisplayNeutralW;
  const bool exporting = gDisplayValues.froniusup && grid < -gridDisplayNeutralW;

  int availablePower = heaterPower - grid;
  if (availablePower < 0) availablePower = 0;
  const int gaugePower =
      gDisplayValues.froniusup ? (importing ? -grid : availablePower) : 0;

  // Header: three independent fields. The clock normally changes only once a
  // minute; water temperature and CE status update only when their value/state
  // actually changes.
  String clockText = timeClient.getFormattedTime();
  if (clockText.length() >= 5) clockText = clockText.substring(0, 5);
  if (fullRedraw || !gSmoothDashboardCache.valid ||
      gSmoothDashboardCache.clockText != clockText) {
    display.fillRect(0, 0, 56, 19, TFT_BLACK);
    display.setTextFont(2);
    display.setTextSize(1);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setCursor(2, 2, 2);
    display.print(clockText);
    gSmoothDashboardCache.clockText = clockText;
  }

  const int tempColor = waterTemperatureColorTTGO(waterTemp, effectiveMaxTemp);
  const int tempTenths = tenthsTTGO(waterTemp);
  if (fullRedraw || !gSmoothDashboardCache.valid ||
      gSmoothDashboardCache.tempTenths != tempTenths ||
      gSmoothDashboardCache.maxTemp != effectiveMaxTemp ||
      gSmoothDashboardCache.tempColor != tempColor) {
    display.fillRect(56, 0, 101, 19, TFT_BLACK);
    drawThermometerIcon(59, 1, tempColor);
    const String currentText = waterTemp > 0.0f ? String(waterTemp, 1) : "--.-";
    String maxText = "/";
    maxText += effectiveMaxTemp > 0 ? String(effectiveMaxTemp) : "--";
    display.setTextFont(2);
    display.setTextSize(1);
    display.setTextColor(tempColor, TFT_BLACK);
    display.setCursor(73, 2, 2);
    display.print(currentText);
    const int maxX = 73 + display.textWidth(currentText, 2);
    display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    display.setCursor(maxX, 2, 2);
    display.print(maxText);
    const int degreeX = maxX + display.textWidth(maxText, 2) + 1;
    drawDegreeCUnitTTGO(degreeX, 2, 2, TFT_LIGHTGREY, TFT_BLACK);
    gSmoothDashboardCache.tempTenths = tempTenths;
    gSmoothDashboardCache.maxTemp = effectiveMaxTemp;
    gSmoothDashboardCache.tempColor = tempColor;
  }

  int ceState = 5;
  if (!dimmerFresh) ceState = 0;
  else if (heaterAtTempLimit) ceState = 1;
  else if (heaterTempHold) ceState = 2;
  else if (dimmerStarting) ceState = 3;
  else if (!dimmerSynced) ceState = 4;

  if (fullRedraw || !gSmoothDashboardCache.valid ||
      gSmoothDashboardCache.ceState != ceState) {
    display.fillRect(157, 0, 83, 19, TFT_BLACK);
    const int iconX = 158;
    const int textX = 174;
    display.setTextFont(2);
    display.setTextSize(1);
    if (ceState == 0) {
      drawErrorIcon(iconX, 2, TFT_RED);
      display.setTextColor(TFT_RED, TFT_BLACK);
      display.setCursor(textX, 2, 2); display.print("CE ERR");
    }
    else if (ceState == 1) {
      drawCheckIcon(iconX, 2, TFT_ORANGE);
      display.setTextColor(TFT_ORANGE, TFT_BLACK);
      display.setCursor(textX, 2, 2); display.print("TEMP MAX");
    }
    else if (ceState == 2) {
      drawClockIcon(iconX, 2, TFT_ORANGE);
      display.setTextColor(TFT_ORANGE, TFT_BLACK);
      display.setCursor(textX, 2, 2); display.print("TEMP HOLD");
    }
    else if (ceState == 3) {
      drawClockIcon(iconX, 2, TFT_YELLOW);
      display.setTextColor(TFT_YELLOW, TFT_BLACK);
      display.setCursor(textX, 2, 2); display.print("CE START");
    }
    else if (ceState == 4) {
      drawClockIcon(iconX, 2, TFT_YELLOW);
      display.setTextColor(TFT_YELLOW, TFT_BLACK);
      display.setCursor(textX, 2, 2); display.print("CE SYNC");
    }
    else {
      drawCheckIcon(iconX, 2, TFT_GREEN);
      display.setTextColor(TFT_WHITE, TFT_BLACK);
      display.setCursor(textX, 2, 2); display.print("CE OK");
    }
    gSmoothDashboardCache.ceState = ceState;
  }

  if (gDisplayValues.froniusup && !importing) {
    if (gHighSurplusBand) {
      if (availablePower < 1950) gHighSurplusBand = false;
    }
    else if (availablePower >= 2050) {
      gHighSurplusBand = true;
    }
  }
  else {
    gHighSurplusBand = false;
  }

  int bannerMode = 0;
  int bannerBg = TFT_DARKGREY;
  int bannerFg = TFT_WHITE;
  int bannerWatts = 0;
  if (gDisplayValues.froniusup) {
    if (importing) {
      bannerMode = 1; bannerBg = TFT_RED; bannerFg = TFT_WHITE; bannerWatts = grid;
    }
    else if (availablePower <= gridDisplayNeutralW) {
      bannerMode = 2; bannerBg = TFT_CYAN; bannerFg = TFT_BLACK; bannerWatts = availablePower;
    }
    else if (heaterTempHold && exporting) {
      bannerMode = 4;
      bannerBg = gHighSurplusBand ? TFT_GREEN : TFT_ORANGE;
      bannerFg = TFT_BLACK;
      bannerWatts = -grid;
    }
    else {
      bannerMode = 3;
      bannerBg = gHighSurplusBand ? TFT_GREEN : TFT_ORANGE;
      bannerFg = TFT_BLACK;
      bannerWatts = availablePower;
    }
  }
  drawSmoothBannerTTGO(bannerMode, bannerBg, bannerFg, bannerWatts, fullRedraw);

  // PV field.
  if (fullRedraw || !gSmoothDashboardCache.valid ||
      gSmoothDashboardCache.pvWatts != pvPower) {
    display.fillRect(0, 68, 118, 20, TFT_BLACK);
    display.setTextSize(1); display.setTextFont(2);
    drawSunIcon(2, 69, TFT_YELLOW);
    display.setTextColor(TFT_GREEN, TFT_BLACK);
    display.setCursor(20, 70, 2);
    display.print("PV ");
    display.print(formatPowerTTGO(pvPower));
    gSmoothDashboardCache.pvWatts = pvPower;
  }

  int gridMode = importing ? 2 : (exporting ? 1 : 0);
  if (fullRedraw || !gSmoothDashboardCache.valid ||
      gSmoothDashboardCache.gridWatts != grid ||
      gSmoothDashboardCache.gridMode != gridMode) {
    display.fillRect(118, 68, 122, 20, TFT_BLACK);
    display.setTextSize(1); display.setTextFont(2);
    if (gridMode == 1) {
      drawGridArrowIcon(119, 71, true, TFT_CYAN);
      display.setTextColor(TFT_CYAN, TFT_BLACK);
      display.setCursor(135, 70, 2);
      display.print("EXP "); display.print(formatPowerTTGO(-grid));
    }
    else if (gridMode == 2) {
      drawGridArrowIcon(119, 71, false, TFT_RED);
      display.setTextColor(TFT_RED, TFT_BLACK);
      display.setCursor(135, 70, 2);
      display.print("IMP "); display.print(formatPowerTTGO(grid));
    }
    else {
      display.setTextColor(TFT_WHITE, TFT_BLACK);
      display.setCursor(135, 70, 2); display.print("GRID 0 W");
    }
    gSmoothDashboardCache.gridWatts = grid;
    gSmoothDashboardCache.gridMode = gridMode;
  }

  if (fullRedraw || !gSmoothDashboardCache.valid ||
      gSmoothDashboardCache.dimmerPercent != reportedDimmer ||
      gSmoothDashboardCache.heaterWatts != heaterPower) {
    display.fillRect(0, 89, 128, 18, TFT_BLACK);
    display.setTextSize(1); display.setTextFont(2);
    drawHeaterIcon(2, 90, TFT_ORANGE);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setCursor(20, 91, 2); display.printf("CE %d%%", reportedDimmer);
    display.setCursor(87, 91, 2); display.printf("%dW", heaterPower);
    gSmoothDashboardCache.dimmerPercent = reportedDimmer;
    gSmoothDashboardCache.heaterWatts = heaterPower;
  }

  int ceRightMode = 0;
  if (heaterTempHold) ceRightMode = 1;
  else if (dimmerStarting) ceRightMode = 2;
  else if (gDisplayValues.froniusup) ceRightMode = 3;
  const int releaseTenths = tenthsTTGO(gDisplayValues.dimmerReleaseTemp);

  const bool ceRightChanged =
      fullRedraw || !gSmoothDashboardCache.valid ||
      gSmoothDashboardCache.ceRightMode != ceRightMode ||
      (ceRightMode == 1 && gSmoothDashboardCache.releaseTenths != releaseTenths) ||
      (ceRightMode == 3 && gSmoothDashboardCache.houseWatts != housePower);

  if (ceRightChanged) {
    display.fillRect(128, 89, 112, 18, TFT_BLACK);
    display.setTextSize(1); display.setTextFont(2);
    if (ceRightMode == 1) {
      display.setTextColor(TFT_ORANGE, TFT_BLACK);
      display.setCursor(129, 91, 2);
      display.print("REPRISE ");
      const String releaseText =
          gDisplayValues.dimmerReleaseTemp > 0.0f
              ? String(gDisplayValues.dimmerReleaseTemp, 0)
              : "--";
      display.print(releaseText);
      const int degreeX = 129 + display.textWidth("REPRISE ", 2) +
                          display.textWidth(releaseText, 2) + 1;
      drawDegreeCUnitTTGO(degreeX, 91, 2, TFT_ORANGE, TFT_BLACK);
    }
    else if (ceRightMode == 2) {
      display.setTextColor(TFT_YELLOW, TFT_BLACK);
      display.setCursor(145, 91, 2); display.print("START");
    }
    else if (ceRightMode == 3) {
      drawHouseIcon(133, 90, TFT_WHITE);
      display.setTextColor(TFT_WHITE, TFT_BLACK);
      display.setCursor(151, 91, 2);
      display.print(formatPowerTTGO(housePower));
    }
    gSmoothDashboardCache.ceRightMode = ceRightMode;
    gSmoothDashboardCache.releaseTenths = releaseTenths;
    gSmoothDashboardCache.houseWatts = housePower;
  }

  drawSmoothGaugeTTGO(gaugePower, gDisplayValues.froniusup, fullRedraw);
  gSmoothDashboardCache.valid = true;
}

struct SmoothDiagnosticCache {
  bool valid = false;
  int rssi = 0;
  int wifiColor = 0;
  String ip;
  int fronius = -1;
  int commanded = -1;
  int reported = -1;
  int dimmerFresh = -1;
  long linkAge = -1;
  int tempTenths = 0;
  int maxTemp = 0;
  int tempColor = 0;
  String uptimeText;
};

static SmoothDiagnosticCache gSmoothDiagnosticCache;

static void drawSmoothDiagnosticLabel(const char *label, int y)
{
  display.setTextFont(2);
  display.setTextSize(1);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setCursor(3, y, 2);
  display.print(label);
}

static void drawSmoothDiagnosticValue(const String &value, int y, int color)
{
  display.fillRect(78, y, 162, 16, TFT_BLACK);
  display.setTextFont(2);
  display.setTextSize(1);
  display.setTextColor(color, TFT_BLACK);
  display.setCursor(79, y, 2);
  display.print(value);
}

static void drawTTGOSmoothDiagnostic(bool fullRedraw)
{
  const unsigned long now = millis();
  const bool dimmerFresh =
      gDisplayValues.dimmerCommOk &&
      gDisplayValues.dimmerLastOkMs > 0 &&
      ((unsigned long)(now - gDisplayValues.dimmerLastOkMs) <= 15000UL);

  if (fullRedraw || !gSmoothDiagnosticCache.valid) {
    drawCenteredTTGO("DIAGNOSTIC 2/3", 1, 2, TFT_CYAN);
    display.drawFastHLine(0, 18, 240, TFT_DARKGREY);
    drawSmoothDiagnosticLabel("WiFi", 22);
    drawSmoothDiagnosticLabel("IP", 38);
    drawSmoothDiagnosticLabel("Fronius", 54);
    drawSmoothDiagnosticLabel("Dimmer", 70);
    drawSmoothDiagnosticLabel("CE link", 86);
    drawSmoothDiagnosticLabel("Eau/Tmax", 102);
    drawSmoothDiagnosticLabel("Uptime", 118);
  }

  const int rssi = WiFi.isConnected() ? WiFi.RSSI() : -127;
  int wifiColor = TFT_RED;
  if (rssi >= -60) wifiColor = TFT_GREEN;
  else if (rssi >= -75) wifiColor = TFT_YELLOW;
  if (fullRedraw || !gSmoothDiagnosticCache.valid ||
      gSmoothDiagnosticCache.rssi != rssi ||
      gSmoothDiagnosticCache.wifiColor != wifiColor) {
    drawSmoothDiagnosticValue(String(rssi) + " dBm", 22, wifiColor);
    gSmoothDiagnosticCache.rssi = rssi;
    gSmoothDiagnosticCache.wifiColor = wifiColor;
  }

  if (fullRedraw || !gSmoothDiagnosticCache.valid ||
      gSmoothDiagnosticCache.ip != gDisplayValues.IP) {
    drawSmoothDiagnosticValue(gDisplayValues.IP, 38, TFT_WHITE);
    gSmoothDiagnosticCache.ip = gDisplayValues.IP;
  }

  const int fronius = gDisplayValues.froniusup ? 1 : 0;
  if (fullRedraw || !gSmoothDiagnosticCache.valid ||
      gSmoothDiagnosticCache.fronius != fronius) {
    drawSmoothDiagnosticValue(fronius ? "OK" : "ERREUR", 54,
                              fronius ? TFT_GREEN : TFT_RED);
    gSmoothDiagnosticCache.fronius = fronius;
  }

  const int commanded = constrain(gDisplayValues.dimmer, 0, 100);
  const int reported = constrain(gDisplayValues.dimmerReported, 0, 100);
  if (fullRedraw || !gSmoothDiagnosticCache.valid ||
      gSmoothDiagnosticCache.commanded != commanded ||
      gSmoothDiagnosticCache.reported != reported ||
      gSmoothDiagnosticCache.dimmerFresh != (dimmerFresh ? 1 : 0)) {
    drawSmoothDiagnosticValue(String(commanded) + "% > " + String(reported) + "%",
                              70, dimmerFresh ? TFT_GREEN : TFT_RED);
    gSmoothDiagnosticCache.commanded = commanded;
    gSmoothDiagnosticCache.reported = reported;
    gSmoothDiagnosticCache.dimmerFresh = dimmerFresh ? 1 : 0;
  }

  long linkAge = -1;
  String linkText = "jamais";
  if (gDisplayValues.dimmerLastOkMs > 0) {
    linkAge = (long)((unsigned long)(now - gDisplayValues.dimmerLastOkMs) / 1000UL);
    linkText = String(linkAge) + " s";
  }
  if (fullRedraw || !gSmoothDiagnosticCache.valid ||
      gSmoothDiagnosticCache.linkAge != linkAge) {
    drawSmoothDiagnosticValue(linkText, 86,
                              dimmerFresh ? TFT_GREEN : TFT_RED);
    gSmoothDiagnosticCache.linkAge = linkAge;
  }

  const float waterTemp = gDisplayValues.temperature.toFloat();
  const int effectiveMaxTemp =
      gDisplayValues.dimmerMaxTemp > 0 ? gDisplayValues.dimmerMaxTemp : config.tmax;
  const int tempColor = waterTemperatureColorTTGO(waterTemp, effectiveMaxTemp);
  const int tempTenths = tenthsTTGO(waterTemp);
  if (fullRedraw || !gSmoothDiagnosticCache.valid ||
      gSmoothDiagnosticCache.tempTenths != tempTenths ||
      gSmoothDiagnosticCache.maxTemp != effectiveMaxTemp ||
      gSmoothDiagnosticCache.tempColor != tempColor) {
    display.fillRect(78, 102, 162, 16, TFT_BLACK);
    display.setTextFont(2); display.setTextSize(1);
    int valueX = 79;
    const String currentText = waterTemp > 0.0f ? String(waterTemp, 1) : "--.-";
    display.setTextColor(tempColor, TFT_BLACK);
    display.setCursor(valueX, 102, 2); display.print(currentText);
    valueX += display.textWidth(currentText, 2) + 1;
    valueX = drawDegreeCUnitTTGO(valueX, 102, 2, tempColor, TFT_BLACK);
    display.setTextColor(tempColor, TFT_BLACK);
    display.setCursor(valueX + 2, 102, 2); display.print(" / ");
    valueX += 2 + display.textWidth(" / ", 2);
    const String maxText = effectiveMaxTemp > 0 ? String(effectiveMaxTemp) : "--";
    display.setCursor(valueX, 102, 2); display.print(maxText);
    valueX += display.textWidth(maxText, 2) + 1;
    drawDegreeCUnitTTGO(valueX, 102, 2, tempColor, TFT_BLACK);
    gSmoothDiagnosticCache.tempTenths = tempTenths;
    gSmoothDiagnosticCache.maxTemp = effectiveMaxTemp;
    gSmoothDiagnosticCache.tempColor = tempColor;
  }

  const String uptimeText = formatUptimeTTGO(now);
  if (fullRedraw || !gSmoothDiagnosticCache.valid ||
      gSmoothDiagnosticCache.uptimeText != uptimeText) {
    drawSmoothDiagnosticValue(uptimeText, 118, TFT_WHITE);
    gSmoothDiagnosticCache.uptimeText = uptimeText;
  }

  gSmoothDiagnosticCache.valid = true;
}

void updateDisplaySmooth(void * parameter)
{
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
      // A force refresh now means "render immediately", not "clear the TFT".
      // Only a real page transition clears the whole panel. This avoids the
      // periodic flash previously triggered by RobotDyn telemetry updates.
      gDisplayForceRefresh = false;
      const uint8_t page = gDisplayPage;
      const bool pageChanged = (page != lastPage);
      const bool fullRedraw = pageChanged;

      if (pageChanged) {
        display.fillScreen(TFT_BLACK);
        lastPage = page;
      }

      if (page == 0) {
        drawTTGOSmoothDashboard(fullRedraw);
      }
      else if (page == 1) {
        drawTTGOSmoothDiagnostic(fullRedraw);
      }
      else if (page == 2 && fullRedraw) {
        drawTTGOHelpPage();
      }

      lastDrawMs = now;
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

#endif  // TTGO

#endif  // TASK_SMOOTH_DISPLAY
