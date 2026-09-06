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
  display.drawFastVLine(xZero, y - 2, height + 4, TFT_WHITE);

  if (valid) {
    const int markerX = gaugeXForPowerTTGO(watts);

    // Large solid cursor: a wide white arrow on black plus a 3 px line
    // through the coloured gauge. Deliberately no dark centre so it remains
    // obvious from a distance on every colour zone.
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

  const int heaterPower = (800 * reportedDimmer) / 100;
  const int grid = (int)gDisplayValues.grid;
  const float waterTemp = gDisplayValues.temperature.toFloat();

  int housePower = (int)gDisplayValues.production + grid;
  if (housePower < 0) housePower = 0;

  const bool tempAtOrAboveMax =
      (waterTemp > 0.0f) &&
      (config.tmax > 0) &&
      (waterTemp >= (float)config.tmax);

  const bool heaterAtTempLimit =
      dimmerFresh &&
      tempAtOrAboveMax &&
      (commandedDimmer > 0) &&
      (reportedDimmer == 0);

  const bool dimmerSynced =
      dimmerFresh &&
      (abs(commandedDimmer - reportedDimmer) <= 2);

  int availablePower = heaterPower - grid;
  if (availablePower < 0) availablePower = 0;

  int gaugePower = 0;
  if (gDisplayValues.froniusup) {
    gaugePower = (grid > 0) ? -grid : availablePower;
  }

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

  const int ceIconX = 164;
  const int ceTextX = 180;

  if (!dimmerFresh) {
    drawErrorIcon(ceIconX, 2, TFT_RED);
    display.setTextColor(TFT_RED, TFT_BLACK);
    display.setCursor(ceTextX, 2, 2);
    display.print("CE ERR");
  }
  else if (!dimmerSynced && !heaterAtTempLimit) {
    drawClockIcon(ceIconX, 2, TFT_YELLOW);
    display.setTextColor(TFT_YELLOW, TFT_BLACK);
    display.setCursor(ceTextX, 2, 2);
    display.print("CE SYNC");
  }
  else {
    drawCheckIcon(ceIconX, 2, TFT_GREEN);
    display.setTextColor(TFT_GREEN, TFT_BLACK);
    display.setCursor(ceTextX, 2, 2);
    display.print("CE OK");
  }

  const bool importing = gDisplayValues.froniusup && (grid > 0);
  drawCenteredTTGO(importing ? "IMPORT" : "DISPO",
                   20,
                   2,
                   importing ? TFT_RED : TFT_WHITE);

  int mainColor = TFT_RED;
  if (!importing) {
    if (availablePower >= 2000) mainColor = TFT_GREEN;
    else if (availablePower >= 500) mainColor = TFT_YELLOW;
  }

  String mainText = "---";
  if (gDisplayValues.froniusup) {
    mainText = importing ? formatPowerTTGO(grid)
                         : formatPowerTTGO(availablePower);
  }

  display.setTextFont(2);
  display.setTextSize(2);
  display.setTextColor(mainColor, TFT_BLACK);
  int mainX = (240 - display.textWidth(mainText, 2)) / 2;
  if (mainX < 0) mainX = 0;
  display.setCursor(mainX, 36, 2);
  display.print(mainText);
  display.setTextSize(1);

  display.setTextFont(2);
  drawSunIcon(2, 72, TFT_YELLOW);
  display.setTextColor(TFT_GREEN, TFT_BLACK);
  display.setCursor(20, 73, 2);
  display.print("PV ");
  display.print(formatPowerTTGO((int)gDisplayValues.production));

  if (grid < 0) {
    drawGridArrowIcon(119, 74, true, TFT_CYAN);
    display.setTextColor(TFT_CYAN, TFT_BLACK);
    display.setCursor(135, 73, 2);
    display.print("EXP ");
    display.print(formatPowerTTGO(-grid));
  }
  else {
    drawGridArrowIcon(119, 74, false, TFT_RED);
    display.setTextColor(TFT_RED, TFT_BLACK);
    display.setCursor(135, 73, 2);
    display.print("IMP ");
    display.print(formatPowerTTGO(grid));
  }

  drawHeaterIcon(2, 94, TFT_ORANGE);
  display.setCursor(20, 95, 2);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.printf("CE %d%%", reportedDimmer);
  display.setCursor(87, 95, 2);
  display.printf("%dW", heaterPower);

  if (heaterAtTempLimit) {
    display.setCursor(133, 95, 2);
    display.setTextColor(TFT_ORANGE, TFT_BLACK);
    display.print("TEMP MAX");
  }
  else if (dimmerFresh && !dimmerSynced) {
    display.setCursor(145, 95, 2);
    display.setTextColor(TFT_YELLOW, TFT_BLACK);
    display.print("SYNC");
  }
  else if (gDisplayValues.froniusup) {
    drawHouseIcon(133, 94, TFT_WHITE);
    display.setCursor(151, 95, 2);
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

void updateDisplay(void * parameter){
#ifdef TTGO
  unsigned long lastDrawMs = 0;
#endif

  for (;;){
#ifdef TTGO
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