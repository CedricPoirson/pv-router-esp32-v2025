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
extern TFT_eSPI display;
#endif

extern DisplayValues gDisplayValues;

#ifdef TTGO

static String formatPowerTTGO(int watts)
{
  if (watts < 0) watts = -watts;

  if (watts >= 1000) {
    return String(watts / 1000.0f, 2) + " kW";
  }

  return String(watts) + " W";
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

// Slightly enlarged vector icons for better readability on the 240x135 TTGO.
// No special font or bitmap is required.
static void drawThermometerIcon(int x, int y, int color)
{
  display.drawRoundRect(x + 2, y, 5, 11, 2, color);
  display.drawLine(x + 4, y + 3, x + 4, y + 12, color);
  display.drawCircle(x + 4, y + 13, 4, color);
  display.fillCircle(x + 4, y + 13, 2, color);
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

static void drawClockIcon(int x, int y, int color)
{
  display.drawCircle(x + 6, y + 6, 6, color);
  display.drawLine(x + 6, y + 6, x + 6, y + 2, color);
  display.drawLine(x + 6, y + 6, x + 10, y + 8, color);
}

static void drawWasherIcon(int x, int y, int color)
{
  display.drawRoundRect(x, y, 13, 13, 2, color);
  display.drawLine(x + 2, y + 3, x + 10, y + 3, color);
  display.drawCircle(x + 6, y + 8, 4, color);
  display.drawCircle(x + 6, y + 8, 2, color);
}

static void drawErrorIcon(int x, int y, int color)
{
  display.drawCircle(x + 6, y + 6, 6, color);
  display.drawLine(x + 3, y + 3, x + 9, y + 9, color);
  display.drawLine(x + 9, y + 3, x + 3, y + 9, color);
}

static void drawAdviceTTGO(const String &text, int color, int iconType)
{
  display.setTextFont(2);
  display.setTextSize(1);
  const int iconWidth = 17;
  const int textWidth = display.textWidth(text, 2);
  int x = (240 - (iconWidth + textWidth)) / 2;
  if (x < 0) x = 0;

  if (iconType == 2) drawWasherIcon(x, 116, color);
  else if (iconType == 1) drawClockIcon(x, 116, color);
  else drawErrorIcon(x, 116, color);

  display.setTextColor(color, TFT_BLACK);
  display.setCursor(x + iconWidth, 115, 2);
  display.print(text);
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
      ((unsigned long)(now - gDisplayValues.dimmerLastOkMs) <= 45000UL);

  // Use the power actually reported by the dimmer when its /state endpoint
  // is fresh. If communication is lost, only the current grid export is
  // counted as available power so the displayed value stays conservative.
  int reportedDimmer = dimmerFresh ? gDisplayValues.dimmerReported : 0;
  if (reportedDimmer < 0) reportedDimmer = 0;
  if (reportedDimmer > 100) reportedDimmer = 100;

  const int heaterPower = (800 * reportedDimmer) / 100;
  const int grid = (int)gDisplayValues.grid;

  // Extra household load that can be switched on without importing:
  // present grid export plus power that can be released by reducing the
  // water-heater dimmer. With a lost dimmer link, heaterPower is forced to 0.
  int availablePower = heaterPower - grid;
  if (availablePower < 0) availablePower = 0;

  // -------- Top status line --------
  display.setTextFont(2);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setCursor(2, 2, 2);
  String clockText = timeClient.getFormattedTime();
  if (clockText.length() >= 5) clockText = clockText.substring(0, 5);
  display.print(clockText);

  float waterTemp = gDisplayValues.temperature.toFloat();
  drawThermometerIcon(59, 1, TFT_CYAN);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  if (waterTemp > 0.0f) {
    String tempText = String(waterTemp, 1);
    const int tempX = 73;
    display.setCursor(tempX, 2, 2);
    display.print(tempText);
    const int tempWidth = display.textWidth(tempText, 2);
    display.drawCircle(tempX + tempWidth + 3, 4, 1, TFT_WHITE);
    display.setCursor(tempX + tempWidth + 7, 2, 2);
    display.print("C");
  }
  else {
    display.setCursor(73, 2, 2);
    display.print("--.- C");
  }

  if (dimmerFresh) {
    drawCheckIcon(174, 2, TFT_GREEN);
    display.setTextColor(TFT_GREEN, TFT_BLACK);
    display.setCursor(190, 2, 2);
    display.print("CE OK");
  }
  else {
    drawErrorIcon(174, 2, TFT_RED);
    display.setTextColor(TFT_RED, TFT_BLACK);
    display.setCursor(190, 2, 2);
    display.print("CE ERR");
  }

  // -------- Main information: power available for another appliance --------
  drawCenteredTTGO("DISPO", 22, 2, TFT_WHITE);

  int availableColor = TFT_RED;
  if (availablePower >= 2200) availableColor = TFT_GREEN;
  else if (availablePower >= 500) availableColor = TFT_YELLOW;

  String availableText = gDisplayValues.froniusup
                           ? formatPowerTTGO(availablePower)
                           : String("---");
  drawCenteredTTGO(availableText, 38, 4, availableColor);

  // -------- Secondary information --------
  display.setTextFont(2);
  drawSunIcon(2, 76, TFT_YELLOW);
  display.setTextColor(TFT_GREEN, TFT_BLACK);
  display.setCursor(20, 77, 2);
  display.print("PV ");
  display.print(formatPowerTTGO((int)gDisplayValues.production));

  if (grid < 0) {
    drawGridArrowIcon(119, 78, true, TFT_CYAN);
    display.setTextColor(TFT_CYAN, TFT_BLACK);
    display.setCursor(135, 77, 2);
    display.print("EXP ");
    display.print(formatPowerTTGO(-grid));
  }
  else {
    drawGridArrowIcon(119, 78, false, TFT_RED);
    display.setTextColor(TFT_RED, TFT_BLACK);
    display.setCursor(135, 77, 2);
    display.print("IMP ");
    display.print(formatPowerTTGO(grid));
  }

  drawHeaterIcon(2, 98, TFT_ORANGE);
  display.setCursor(20, 99, 2);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.printf("CE %d%%", reportedDimmer);
  display.setCursor(87, 99, 2);
  display.printf("%dW", heaterPower);

  // -------- Family-facing recommendation --------
  String advice;
  int adviceColor;
  int adviceIcon = 0;

  if (!gDisplayValues.froniusup) {
    advice = "FRONIUS ERR";
    adviceColor = TFT_RED;
  }
  else if (!dimmerFresh) {
    advice = "CE A VERIFIER";
    adviceColor = TFT_RED;
  }
  else if (availablePower >= 2200) {
    advice = "MACHINE OK";
    adviceColor = TFT_GREEN;
    adviceIcon = 2;
  }
  else if (availablePower >= 500) {
    advice = "ATTENDRE MACHINE";
    adviceColor = TFT_YELLOW;
    adviceIcon = 1;
  }
  else {
    advice = "ATTENDRE";
    adviceColor = TFT_RED;
    adviceIcon = 1;
  }

  drawAdviceTTGO(advice, adviceColor, adviceIcon);
}

#endif

/**
 * Draw the current status on the attached display.
 */
void updateDisplay(void * parameter){
  for (;;){
    serial_println(F("lcd task"));

#ifdef TTGO
    drawTTGOZeroGridDashboard();
#endif

#ifdef DEVKIT1
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
#endif

    // Update every 5 seconds.
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}

#endif
