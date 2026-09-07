#ifndef TASK_UPDATE_DISPLAY
#define TASK_UPDATE_DISPLAY

#include <Arduino.h>
#include "functions/drawFunctions.h"
#include "functions/appweb.h"
#include "../config/config.h"

#ifdef DEVKIT1
#include "SSD1306Wire.h"
extern SSD1306Wire display;
#endif

#ifdef TTGO
#include <TFT_eSPI.h>
#include <WiFi.h>
extern TFT_eSPI display;
#endif

extern DisplayValues gDisplayValues;

#ifdef TTGO

// Page 0 = family dashboard, page 1 = diagnostics.
// switchDisplay.h changes these values after a button press.
volatile uint8_t gDisplayPage = 0;
volatile bool gDisplayForceRefresh = true;
volatile bool gDisplayBootComplete = false;

// Display-only startup window: distinguish a dimmer that has just been
// commanded ON from a persistent command/actual mismatch.
static unsigned long gDimmerStartSinceMs = 0;

// Display-only hysteresis around the 2 kW surplus colour threshold so the
// full-width banner does not flicker when available power hovers near 2 kW.
static bool gHighSurplusBand = false;

static String formatPowerTTGO(int watts)
{
  if (watts < 0) watts = -watts;

  if (watts >= 1000) {
    return String(watts / 1000.0f, 2) + " kW";
  }

  return String(watts) + " W";
}

static String formatUptimeTTGO(unsigned long uptimeMs)
{
  const unsigned long totalMinutes = uptimeMs / 60000UL;
  const unsigned long days = totalMinutes / 1440UL;
  const unsigned long hours = (totalMinutes / 60UL) % 24UL;
  const unsigned long minutes = totalMinutes % 60UL;

  if (days > 0) {
    return String(days) + "j " + String(hours) + "h";
  }

  return String(hours) + "h " + String(minutes) + "m";
}

static void drawCenteredTTGO(const String &text, int y, int font, int color)
{
  display.setTextFont(font);
  display.setTextSize(1);
  display.setTextColor(color, TFT_BLACK);
  int x = (240 - display.textWidth(text, font)) / 2;
  if (x < 0) x = 0;
  display.setCursor(x, y, font);
  display.print(text);
}

// Compact vector boot screen: no bitmap or SPIFFS asset required, so it is
// available immediately after TFT initialisation and also during uploadfs.
static void drawTTGOBootScreen(const String &stage,
                               const String &detail,
                               uint8_t progress)
{
  if (progress > 100) progress = 100;

  display.fillScreen(TFT_BLACK);
  display.setTextSize(1);

  // PV badge.
  display.fillRoundRect(9, 8, 43, 43, 10, TFT_GREEN);
  display.drawRoundRect(9, 8, 43, 43, 10, TFT_CYAN);
  display.setTextFont(4);
  display.setTextColor(TFT_BLACK, TFT_GREEN);
  display.setCursor(15, 17, 4);
  display.print("PV");

  display.setTextFont(4);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setCursor(63, 8, 4);
  display.print("PV ROUTER");

  display.setTextFont(2);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.setCursor(64, 38, 2);
  display.print("ZERO GRID V14.3");

  display.drawFastHLine(10, 61, 220, TFT_DARKGREY);
  drawCenteredTTGO(stage, 70, 2, TFT_WHITE);
  drawCenteredTTGO(detail, 90, 1, TFT_LIGHTGREY);

  const int barX = 14;
  const int barY = 112;
  const int barW = 212;
  const int barH = 9;
  display.drawRoundRect(barX, barY, barW, barH, 4, TFT_DARKGREY);
  const int fillW = ((barW - 4) * progress) / 100;
  if (fillW > 0)
    display.fillRoundRect(barX + 2, barY + 2, fillW, barH - 4, 2,
                          progress >= 100 ? TFT_GREEN : TFT_CYAN);

  display.setTextFont(1);
  display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  const String pct = String(progress) + "%";
  display.setCursor((240 - display.textWidth(pct, 1)) / 2, 125, 1);
  display.print(pct);
}

static void drawDiagnosticRowTTGO(const String &label,
                                  const String &value,
                                  int y,
                                  int valueColor)
{
  display.setTextFont(2);
  display.setTextSize(1);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setCursor(3, y, 2);
  display.print(label);

  display.setTextColor(valueColor, TFT_BLACK);
  display.setCursor(79, y, 2);
  display.print(value);
}

// Draw the degree sign as a tiny vector circle instead of relying on the
// built-in TFT font. The latter does not contain the UTF-8 degree glyph on
// this TTGO, which is why a literal "°C" was rendered as only "C".
// Returns the x coordinate immediately after the C.
static int drawDegreeCUnitTTGO(int x, int y, int font, int color, int bg)
{
  display.setTextFont(font);
  display.setTextSize(1);
  display.setTextColor(color, bg);
  display.drawCircle(x + 2, y + 3, 2, color);

  const int cX = x + 6;
  display.setCursor(cX, y, font);
  display.print("C");
  return cX + display.textWidth("C", font);
}

// Slightly enlarged vector icons for better readability on the 240x135 TTGO.
// No special font or bitmap is required.
static void drawThermometerIcon(int x, int y, int color)
{
  display.drawRoundRect(x + 2, y, 5, 11, 2, color);
  display.drawLine(x + 4, y + 3, x + 4, y + 12, color);
  display.drawCircle(x + 4, y + 13, 4, color);
  display.fillCircle(x + 4, y + 13, 2, color);
}

// Visual ECS temperature bands. This is display-only and has no effect on the
// actual temperature safety logic in gettemp.h.
// <30 C = cold, 30..50 C = warming, 50 C..Tmax = useful/hot, >=Tmax = limit.
static int waterTemperatureColorTTGO(float waterTemp, int effectiveMaxTemp)
{
  if (waterTemp <= 0.0f)
    return TFT_DARKGREY;

  if (effectiveMaxTemp > 0 && waterTemp >= (float)effectiveMaxTemp)
    return TFT_RED;

  if (waterTemp < 30.0f)
    return TFT_CYAN;

  if (waterTemp < 50.0f)
    return TFT_ORANGE;

  return TFT_GREEN;
}

static void drawCheckIcon(int x, int y, int color)
{
  display.drawCircle(x + 6, y + 6, 6, color);
  display.drawLine(x + 3, y + 6, x + 5, y + 8, color);
  display.drawLine(x + 5, y + 8, x + 10, y + 3, color);
}

static void drawSunIcon(int x, int y, int color)
{
  display.drawCircle(x + 7, y + 7, 4, color);
  display.fillCircle(x + 7, y + 7, 2, color);
  display.drawLine(x + 7, y, x + 7, y + 2, color);
  display.drawLine(x + 7, y + 12, x + 7, y + 14, color);
  display.drawLine(x, y + 7, x + 2, y + 7, color);
  display.drawLine(x + 12, y + 7, x + 14, y + 7, color);
  display.drawLine(x + 2, y + 2, x + 4, y + 4, color);
  display.drawLine(x + 10, y + 10, x + 12, y + 12, color);
  display.drawLine(x + 10, y + 4, x + 12, y + 2, color);
  display.drawLine(x + 2, y + 12, x + 4, y + 10, color);
}

static void drawGridArrowIcon(int x, int y, bool exportPower, int color)
{
  const int cy = y + 6;
  if (exportPower) {
    display.drawLine(x, cy, x + 12, cy, color);
    display.drawLine(x + 12, cy, x + 8, cy - 4, color);
    display.drawLine(x + 12, cy, x + 8, cy + 4, color);
  }
  else {
    display.drawLine(x, cy, x + 12, cy, color);
    display.drawLine(x, cy, x + 4, cy - 4, color);
    display.drawLine(x, cy, x + 4, cy + 4, color);
  }
}

static void drawHeaterIcon(int x, int y, int color)
{
  display.drawRoundRect(x, y, 14, 13, 2, color);
  display.drawLine(x + 2, y + 10, x + 4, y + 3, color);
  display.drawLine(x + 4, y + 3, x + 7, y + 10, color);
  display.drawLine(x + 7, y + 10, x + 10, y + 3, color);
  display.drawLine(x + 10, y + 3, x + 12, y + 10, color);
}

static void drawHouseIcon(int x, int y, int color)
{
  display.drawLine(x, y + 6, x + 7, y, color);
  display.drawLine(x + 7, y, x + 14, y + 6, color);
  display.drawRect(x + 2, y + 6, 10, 7, color);
  display.drawRect(x + 6, y + 9, 3, 4, color);
}

static void drawClockIcon(int x, int y, int color)
{
  display.drawCircle(x + 6, y + 6, 6, color);
  display.drawLine(x + 6, y + 6, x + 6, y + 2, color);
  display.drawLine(x + 6, y + 6, x + 10, y + 8, color);
}

static void drawErrorIcon(int x, int y, int color)
{
  display.drawCircle(x + 6, y + 6, 6, color);
  display.drawLine(x + 3, y + 3, x + 9, y + 9, color);
  display.drawLine(x + 9, y + 3, x + 3, y + 9, color);
}

static int gaugeXForPowerTTGO(int watts)
{
  const int gaugeX = 2;
  const int gaugeWidth = 236;
  const int minPower = -2000;
  const int maxPower = 6000;

  if (watts < minPower) watts = minPower;
  if (watts > maxPower) watts = maxPower;

  return gaugeX + ((long)(watts - minPower) * (gaugeWidth - 1)) /
                    (maxPower - minPower);
}

static void drawPowerGaugeTTGO(int watts, bool valid)
{
  const int x = 2;
  const int y = 116;
  const int width = 236;
  const int height = 9;

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

  // Make the 0 W frontier easy to spot while leaving the moving cursor dominant.
  display.fillRect(xZero - 1, y - 2, 2, height + 4, TFT_LIGHTGREY);

  if (valid) {
    const int markerX = gaugeXForPowerTTGO(watts);

    // Large solid cursor: a wide white arrow plus a 3 px line through the gauge.
    display.fillTriangle(markerX - 7, y - 10,
                         markerX + 7, y - 10,
                         markerX, y - 1,
                         TFT_WHITE);
    display.fillRect(markerX - 1, y - 1, 3, height + 2, TFT_WHITE);
  }

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

static void drawTTGOZeroGridDashboard()
{
  display.fillScreen(TFT_BLACK);
  display.setTextSize(1);

  if (gDisplayValues.currentState != UP) {
    drawCenteredTTGO("NO WIFI", 48, 4, TFT_RED);
    return;
  }

  const unsigned long now = millis();
  const bool dimmerFresh =
      gDisplayValues.dimmerCommOk &&
      gDisplayValues.dimmerLastOkMs > 0 &&
      ((unsigned long)(now - gDisplayValues.dimmerLastOkMs) <= 15000UL);

  int reportedDimmer = dimmerFresh ? gDisplayValues.dimmerReported : 0;
  if (reportedDimmer < 0) reportedDimmer = 0;
  if (reportedDimmer > 100) reportedDimmer = 100;

  int commandedDimmer = gDisplayValues.dimmer;
  if (commandedDimmer < 0) commandedDimmer = 0;
  if (commandedDimmer > 100) commandedDimmer = 100;

  const int configuredHeaterPowerW =
      constrain(config.heaterPowerW > 0 ? config.heaterPowerW : 800, 100, 5000);
  const int heaterPower = (configuredHeaterPowerW * reportedDimmer) / 100;
  const int grid = (int)gDisplayValues.grid;
  const float waterTemp = gDisplayValues.temperature.toFloat();
  const int effectiveMaxTemp =
      gDisplayValues.dimmerMaxTemp > 0
          ? gDisplayValues.dimmerMaxTemp
          : config.tmax;

  int housePower = (int)gDisplayValues.production + grid;
  if (housePower < 0) housePower = 0;

  const bool tempAtOrAboveMax =
      (waterTemp > 0.0f) &&
      (effectiveMaxTemp > 0) &&
      (waterTemp >= (float)effectiveMaxTemp);
  const bool heaterTempHold =
      dimmerFresh && gDisplayValues.dimmerTempLimitActive;

  // The top status distinguishes the actual threshold from the hysteresis hold.
  const bool heaterAtTempLimit = heaterTempHold && tempAtOrAboveMax;

  const bool dimmerSynced =
      dimmerFresh &&
      (abs(commandedDimmer - reportedDimmer) <= 2);

  // A fresh positive command with an almost-zero actual output is a normal
  // startup for a short time. Do not keep resetting this timer when the Zero
  // Grid command changes while the dimmer is still starting.
  if (dimmerFresh && commandedDimmer > 0 && reportedDimmer < 5 && !heaterTempHold) {
    if (gDimmerStartSinceMs == 0)
      gDimmerStartSinceMs = now;
  }
  else {
    gDimmerStartSinceMs = 0;
  }

  const bool dimmerStarting =
      gDimmerStartSinceMs > 0 &&
      ((unsigned long)(now - gDimmerStartSinceMs) <= 15000UL);

  // Display-only neutral zone. It does not change the Zero Grid regulation.
  const int gridDisplayNeutralW = 20;
  const bool importing =
      gDisplayValues.froniusup && (grid > gridDisplayNeutralW);
  const bool exporting =
      gDisplayValues.froniusup && (grid < -gridDisplayNeutralW);

  int availablePower = heaterPower - grid;
  if (availablePower < 0) availablePower = 0;

  int gaugePower = 0;
  if (gDisplayValues.froniusup) {
    gaugePower = importing ? -grid : availablePower;
  }

  display.setTextFont(2);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setCursor(2, 2, 2);
  String clockText = timeClient.getFormattedTime();
  if (clockText.length() >= 5) clockText = clockText.substring(0, 5);
  display.print(clockText);

  const int tempColor = waterTemperatureColorTTGO(waterTemp, effectiveMaxTemp);
  drawThermometerIcon(59, 1, tempColor);
  const String tempCurrentText =
      waterTemp > 0.0f ? String(waterTemp, 1) : "--.-";
  String tempMaxText = "/";
  tempMaxText += effectiveMaxTemp > 0 ? String(effectiveMaxTemp) : "--";

  display.setTextColor(tempColor, TFT_BLACK);
  display.setCursor(73, 2, 2);
  display.print(tempCurrentText);

  const int tempMaxX = 73 + display.textWidth(tempCurrentText, 2);
  display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  display.setCursor(tempMaxX, 2, 2);
  display.print(tempMaxText);

  const int tempDegreeX = tempMaxX + display.textWidth(tempMaxText, 2) + 1;
  drawDegreeCUnitTTGO(tempDegreeX, 2, 2, TFT_LIGHTGREY, TFT_BLACK);

  const int ceIconX = 158;
  const int ceTextX = 174;

  if (!dimmerFresh) {
    drawErrorIcon(ceIconX, 2, TFT_RED);
    display.setTextColor(TFT_RED, TFT_BLACK);
    display.setCursor(ceTextX, 2, 2);
    display.print("CE ERR");
  }
  else if (heaterAtTempLimit) {
    drawCheckIcon(ceIconX, 2, TFT_ORANGE);
    display.setTextColor(TFT_ORANGE, TFT_BLACK);
    display.setCursor(ceTextX, 2, 2);
    display.print("TEMP MAX");
  }
  else if (heaterTempHold) {
    drawClockIcon(ceIconX, 2, TFT_ORANGE);
    display.setTextColor(TFT_ORANGE, TFT_BLACK);
    display.setCursor(ceTextX, 2, 2);
    display.print("TEMP HOLD");
  }
  else if (dimmerStarting) {
    drawClockIcon(ceIconX, 2, TFT_YELLOW);
    display.setTextColor(TFT_YELLOW, TFT_BLACK);
    display.setCursor(ceTextX, 2, 2);
    display.print("CE START");
  }
  else if (!dimmerSynced) {
    drawClockIcon(ceIconX, 2, TFT_YELLOW);
    display.setTextColor(TFT_YELLOW, TFT_BLACK);
    display.setCursor(ceTextX, 2, 2);
    display.print("CE SYNC");
  }
  else {
    drawCheckIcon(ceIconX, 2, TFT_GREEN);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setCursor(ceTextX, 2, 2);
    display.print("CE OK");
  }

  // Full-width, high-contrast power banner for easy reading from a distance.
  // Red = grid import, cyan = near-zero, orange = 0..2 kW available/surplus,
  // green = >2 kW. The 2 kW boundary uses a 100 W hysteresis.
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

  int bannerBg = TFT_DARKGREY;
  int bannerFg = TFT_WHITE;
  String bannerLabel = "FRONIUS";
  String bannerValue = "---";

  if (gDisplayValues.froniusup) {
    if (importing) {
      bannerBg = TFT_RED;
      bannerFg = TFT_WHITE;
      bannerLabel = "IMPORT";
      bannerValue = formatPowerTTGO(grid);
    }
    else if (availablePower <= gridDisplayNeutralW) {
      bannerBg = TFT_CYAN;
      bannerFg = TFT_BLACK;
      bannerLabel = "ZERO GRID";
      bannerValue = formatPowerTTGO(availablePower);
    }
    else {
      bannerBg = gHighSurplusBand ? TFT_GREEN : TFT_ORANGE;
      bannerFg = TFT_BLACK;
      if (heaterTempHold && exporting) {
        bannerLabel = "SURPLUS";
        bannerValue = formatPowerTTGO(-grid);
      }
      else {
        bannerLabel = "DISPO";
        bannerValue = formatPowerTTGO(availablePower);
      }
    }
  }

  const int bannerY = 20;
  const int bannerH = 47;
  display.fillRect(0, bannerY, 240, bannerH, bannerBg);

  // Make DISPO/SURPLUS more prominent without stealing space from the main kW
  // value. Other banner states keep their existing compact label style.
  const bool largeBannerLabel =
      (bannerLabel == "DISPO" || bannerLabel == "SURPLUS");
  const int bannerLabelFont = largeBannerLabel ? 1 : 2;
  const int bannerLabelSize = largeBannerLabel ? 2 : 1;
  const int bannerLabelY = largeBannerLabel ? bannerY + 1 : bannerY + 2;
  display.setTextSize(bannerLabelSize);
  display.setTextFont(bannerLabelFont);
  display.setTextColor(bannerFg, bannerBg);
  int bannerLabelX =
      (240 - display.textWidth(bannerLabel, bannerLabelFont)) / 2;
  if (bannerLabelX < 0) bannerLabelX = 0;
  display.setCursor(bannerLabelX, bannerLabelY, bannerLabelFont);
  display.print(bannerLabel);

  // Keep the large font that already fits every W/kW value, but render a
  // second transparent 1 px pass to give the power value a slightly bolder,
  // more prominent appearance without upsetting the layout.
  display.setTextSize(1);
  display.setTextFont(4);
  display.setTextColor(bannerFg, bannerBg);
  const int bannerValueWidth = display.textWidth(bannerValue, 4) + 1;
  int bannerValueX = (240 - bannerValueWidth) / 2;
  if (bannerValueX < 0) bannerValueX = 0;
  const int bannerValueY = bannerY + 18;
  display.setCursor(bannerValueX, bannerValueY, 4);
  display.print(bannerValue);
  display.setTextColor(bannerFg);
  display.setCursor(bannerValueX + 1, bannerValueY, 4);
  display.print(bannerValue);

  display.setTextSize(1);
  display.setTextFont(2);
  drawSunIcon(2, 69, TFT_YELLOW);
  display.setTextColor(TFT_GREEN, TFT_BLACK);
  display.setCursor(20, 70, 2);
  display.print("PV ");
  display.print(formatPowerTTGO((int)gDisplayValues.production));

  if (exporting) {
    drawGridArrowIcon(119, 71, true, TFT_CYAN);
    display.setTextColor(TFT_CYAN, TFT_BLACK);
    display.setCursor(135, 70, 2);
    display.print("EXP ");
    display.print(formatPowerTTGO(-grid));
  }
  else if (importing) {
    drawGridArrowIcon(119, 71, false, TFT_RED);
    display.setTextColor(TFT_RED, TFT_BLACK);
    display.setCursor(135, 70, 2);
    display.print("IMP ");
    display.print(formatPowerTTGO(grid));
  }
  else {
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setCursor(135, 70, 2);
    display.print("GRID 0 W");
  }

  drawHeaterIcon(2, 90, TFT_ORANGE);
  display.setCursor(20, 91, 2);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.printf("CE %d%%", reportedDimmer);
  display.setCursor(87, 91, 2);
  display.printf("%dW", heaterPower);

  if (heaterTempHold) {
    display.setTextColor(TFT_ORANGE, TFT_BLACK);
    display.setCursor(129, 91, 2);
    display.print("REPRISE ");
    const String releaseText =
        gDisplayValues.dimmerReleaseTemp > 0.0f
            ? String(gDisplayValues.dimmerReleaseTemp, 0)
            : "--";
    display.print(releaseText);
    const int releaseDegreeX =
        129 + display.textWidth("REPRISE ", 2) +
        display.textWidth(releaseText, 2) + 1;
    drawDegreeCUnitTTGO(releaseDegreeX, 91, 2, TFT_ORANGE, TFT_BLACK);
  }
  else if (dimmerStarting) {
    display.setCursor(145, 91, 2);
    display.setTextColor(TFT_YELLOW, TFT_BLACK);
    display.print("START");
  }
  else if (gDisplayValues.froniusup) {
    // CE SYNC is already shown in the top status area. Keep the lower-right
    // area useful by continuing to show house consumption during sync.
    drawHouseIcon(133, 90, TFT_WHITE);
    display.setCursor(151, 91, 2);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.print(formatPowerTTGO(housePower));
  }

  drawPowerGaugeTTGO(gaugePower, gDisplayValues.froniusup);
}

static void drawTTGODiagnosticPage()
{
  display.fillScreen(TFT_BLACK);
  display.setTextSize(1);

  drawCenteredTTGO("DIAGNOSTIC V13", 1, 2, TFT_CYAN);
  display.drawFastHLine(0, 18, 240, TFT_DARKGREY);

  const unsigned long now = millis();
  const bool dimmerFresh =
      gDisplayValues.dimmerCommOk &&
      gDisplayValues.dimmerLastOkMs > 0 &&
      ((unsigned long)(now - gDisplayValues.dimmerLastOkMs) <= 15000UL);

  const int rssi = WiFi.isConnected() ? WiFi.RSSI() : -127;
  int wifiColor = TFT_RED;
  if (rssi >= -60) wifiColor = TFT_GREEN;
  else if (rssi >= -75) wifiColor = TFT_YELLOW;

  drawDiagnosticRowTTGO("WiFi", String(rssi) + " dBm", 22, wifiColor);
  drawDiagnosticRowTTGO("IP", gDisplayValues.IP, 38, TFT_WHITE);
  drawDiagnosticRowTTGO("Fronius",
                        gDisplayValues.froniusup ? "OK" : "ERREUR",
                        54,
                        gDisplayValues.froniusup ? TFT_GREEN : TFT_RED);

  int commandedDimmer = gDisplayValues.dimmer;
  if (commandedDimmer < 0) commandedDimmer = 0;
  if (commandedDimmer > 100) commandedDimmer = 100;

  int reportedDimmer = gDisplayValues.dimmerReported;
  if (reportedDimmer < 0) reportedDimmer = 0;
  if (reportedDimmer > 100) reportedDimmer = 100;

  const String dimmerText = String(commandedDimmer) + "% > " +
                            String(reportedDimmer) + "%";
  drawDiagnosticRowTTGO("Dimmer",
                        dimmerText,
                        70,
                        dimmerFresh ? TFT_GREEN : TFT_RED);

  String linkAge = "jamais";
  if (gDisplayValues.dimmerLastOkMs > 0) {
    linkAge = String((unsigned long)(now - gDisplayValues.dimmerLastOkMs) / 1000UL) + " s";
  }
  drawDiagnosticRowTTGO("CE link",
                        linkAge,
                        86,
                        dimmerFresh ? TFT_GREEN : TFT_RED);

  const float waterTemp = gDisplayValues.temperature.toFloat();
  const int effectiveMaxTemp =
      gDisplayValues.dimmerMaxTemp > 0
          ? gDisplayValues.dimmerMaxTemp
          : config.tmax;
  const int tempColor =
      waterTemperatureColorTTGO(waterTemp, effectiveMaxTemp);

  display.setTextFont(2);
  display.setTextSize(1);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setCursor(3, 102, 2);
  display.print("Eau/Tmax");

  int valueX = 79;
  const String currentTempText =
      waterTemp > 0.0f ? String(waterTemp, 1) : "--.-";
  display.setTextColor(tempColor, TFT_BLACK);
  display.setCursor(valueX, 102, 2);
  display.print(currentTempText);
  valueX += display.textWidth(currentTempText, 2) + 1;
  valueX = drawDegreeCUnitTTGO(valueX, 102, 2, tempColor, TFT_BLACK);

  display.setTextColor(tempColor, TFT_BLACK);
  display.setCursor(valueX + 2, 102, 2);
  display.print(" / ");
  valueX += 2 + display.textWidth(" / ", 2);

  const String maxTempText =
      effectiveMaxTemp > 0 ? String(effectiveMaxTemp) : "--";
  display.setCursor(valueX, 102, 2);
  display.print(maxTempText);
  valueX += display.textWidth(maxTempText, 2) + 1;
  drawDegreeCUnitTTGO(valueX, 102, 2, tempColor, TFT_BLACK);

  drawDiagnosticRowTTGO("Uptime", formatUptimeTTGO(now), 118, TFT_WHITE);
}

#endif

void updateDisplay(void * parameter){
#ifdef TTGO
  unsigned long lastDrawMs = 0;
#endif

  for (;;){
#ifdef TTGO
    if (!gDisplayBootComplete) {
      vTaskDelay(100 / portTICK_PERIOD_MS);
      continue;
    }

    const unsigned long now = millis();
    if (gDisplayForceRefresh ||
        (unsigned long)(now - lastDrawMs) >= 2000UL) {
      gDisplayForceRefresh = false;

      if (gDisplayPage == 0)
        drawTTGOZeroGridDashboard();
      else
        drawTTGODiagnosticPage();

      lastDrawMs = now;
    }

    vTaskDelay(100 / portTICK_PERIOD_MS);

#elif defined(DEVKIT1)
    display.clear();

#if WIFI_ACTIVE == true
    if (gDisplayValues.currentState == UP) {
      drawTime();
      drawIP();
      drawversion();
      drawPowerFromFronius();
    }
    else {
      drawtext10(64, 0, "no Wifi");
    }
#endif

    drawtext10(64, 16, injection_type());
    drawtext16(55, 30, String(gDisplayValues.watt, 0) + " W");
    drawtext16(64, 48, String(gDisplayValues.dimmer) + " %");
    display.display();
    vTaskDelay(5000 / portTICK_PERIOD_MS);

#else
    vTaskDelay(5000 / portTICK_PERIOD_MS);
#endif
  }
}

#endif
