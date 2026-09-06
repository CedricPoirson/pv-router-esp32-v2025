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

  // Power actually reported by the remote dimmer.
  // If its state is stale, do not count heater power as releasable capacity.
  int reportedDimmer = dimmerFresh ? gDisplayValues.dimmerReported : 0;
  if (reportedDimmer < 0) reportedDimmer = 0;
  if (reportedDimmer > 100) reportedDimmer = 100;

  int commandedDimmer = gDisplayValues.dimmer;
  if (commandedDimmer < 0) commandedDimmer = 0;
  if (commandedDimmer > 100) commandedDimmer = 100;

  const int heaterPower = (800 * reportedDimmer) / 100;
  const int grid = (int)gDisplayValues.grid;
  const float waterTemp = gDisplayValues.temperature.toFloat();

  // Fronius convention: P_Grid > 0 import, P_Grid < 0 export.
  // Household consumption therefore equals PV production + P_Grid.
  int housePower = (int)gDisplayValues.production + grid;
  if (housePower < 0) housePower = 0;

  // config.tmax is loaded from the router configuration (for example 55 C).
  const bool tempAtOrAboveMax =
      (waterTemp > 0.0f) &&
      (config.tmax > 0) &&
      (waterTemp >= (float)config.tmax);

  // Strong indication that the dimmer's own temperature protection has
  // deliberately stopped the heater: the router is asking for power, the
  // dimmer answers normally, but reports 0 % while the water is at Tmax.
  const bool heaterAtTempLimit =
      dimmerFresh &&
      tempAtOrAboveMax &&
      (commandedDimmer > 0) &&
      (reportedDimmer == 0);

  // Outside the temperature-cutoff case, a difference between requested and
  // reported dimmer power is shown as CE SYNC rather than falsely saying OK.
  const bool dimmerSynced =
      dimmerFresh &&
      (abs(commandedDimmer - reportedDimmer) <= 2);

  // Extra household load that can be switched on without importing:
  // present grid export plus power that can actually be released from the
  // water heater. If the thermostat has already stopped it, heaterPower is 0.
  int availablePower = heaterPower - grid;
  if (availablePower < 0) availablePower = 0;

  // -------- Top status line --------
  display.setTextFont(2);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setCursor(2, 2, 2);
  String clockText = timeClient.getFormattedTime();
  if (clockText.length() >= 5) clockText = clockText.substring(0, 5);
  display.print(clockText);

  const int tempColor = tempAtOrAboveMax ? TFT_ORANGE : TFT_CYAN;
  drawThermometerIcon(59, 1, tempColor);
  display.setTextColor(tempAtOrAboveMax ? TFT_ORANGE : TFT_WHITE, TFT_BLACK);
  if (waterTemp > 0.0f) {
    String tempText = String(waterTemp, 1);
    const int tempX = 73;
    display.setCursor(tempX, 2, 2);
    display.print(tempText);
    const int tempWidth = display.textWidth(tempText, 2);
    display.drawCircle(tempX + tempWidth + 3, 4, 1,
                       tempAtOrAboveMax ? TFT_ORANGE : TFT_WHITE);
    display.setCursor(tempX + tempWidth + 7, 2, 2);
    display.print("C");
  }
  else {
    display.setCursor(73, 2, 2);
    display.print("--.- C");
  }

  if (!dimmerFresh) {
    drawErrorIcon(174, 2, TFT_RED);
    display.setTextColor(TFT_RED, TFT_BLACK);
    display.setCursor(190, 2, 2);
    display.print("CE ERR");
  }
  else if (!dimmerSynced && !heaterAtTempLimit) {
    drawClockIcon(174, 2, TFT_YELLOW);
    display.setTextColor(TFT_YELLOW, TFT_BLACK);
    display.setCursor(190, 2, 2);
    display.print("CE SYNC");
  }
  else {
    drawCheckIcon(174, 2, TFT_GREEN);
    display.setTextColor(TFT_GREEN, TFT_BLACK);
    display.setCursor(190, 2, 2);
    display.print("CE OK");
  }

  // -------- Main information: power available for another appliance --------
  drawCenteredTTGO("DISPO", 22, 2, TFT_WHITE);

  int availableColor = TFT_RED;
  if (availablePower >= 2000) availableColor = TFT_GREEN;
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

  if (heaterAtTempLimit) {
    display.setCursor(133, 99, 2);
    display.setTextColor(TFT_ORANGE, TFT_BLACK);
    display.print("TEMP MAX");
  }
  else if (dimmerFresh && !dimmerSynced) {
    display.setCursor(145, 99, 2);
    display.setTextColor(TFT_YELLOW, TFT_BLACK);
    display.print("SYNC");
  }
  else if (gDisplayValues.froniusup) {
    drawHouseIcon(133, 98, TFT_WHITE);
    display.setCursor(151, 99, 2);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.print(formatPowerTTGO(housePower));
  }

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
  else if (availablePower >= 2000) {
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
      ((unsigned long)(now - gDisplayValues.dimmerLastOkMs) <= 45000UL);

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
  const String temperatureText = String(config.tmax) + "C / " +
                                 String(waterTemp, 1) + "C";
  const int tempColor =
      (waterTemp > 0.0f && config.tmax > 0 && waterTemp >= config.tmax)
          ? TFT_ORANGE
          : TFT_WHITE;
  drawDiagnosticRowTTGO("Tmax/Eau", temperatureText, 102, tempColor);
  drawDiagnosticRowTTGO("Uptime", formatUptimeTTGO(now), 118, TFT_WHITE);
}

#endif

/**
 * Draw the current status on the attached display.
 */
void updateDisplay(void * parameter){
#ifdef TTGO
  unsigned long lastDrawMs = 0;
#endif

  for (;;){
#ifdef TTGO
    const unsigned long now = millis();
    if (gDisplayForceRefresh ||
        (unsigned long)(now - lastDrawMs) >= 5000UL) {
      serial_println(F("lcd task"));
      gDisplayForceRefresh = false;

      if (gDisplayPage == 0)
        drawTTGOZeroGridDashboard();
      else
        drawTTGODiagnosticPage();

      lastDrawMs = now;
    }

    // Poll frequently so a short button press changes page immediately,
    // while the actual TFT redraw remains at 5 s unless forced.
    vTaskDelay(100 / portTICK_PERIOD_MS);

#elif defined(DEVKIT1)
    serial_println(F("lcd task"));
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
