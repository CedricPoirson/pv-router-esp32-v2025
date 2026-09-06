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

static void drawTTGOZeroGridDashboard()
{
  display.fillScreen(TFT_BLACK);
  display.setTextSize(1);

  if (gDisplayValues.currentState != UP) {
    display.setTextFont(4);
    display.setTextColor(TFT_RED, TFT_BLACK);
    display.setCursor(45, 48, 4);
    display.print("NO WIFI");
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

  // Additional household load that can be switched on without importing:
  // current grid export + power that can be released from the water heater.
  int availablePower = heaterPower - grid;
  if (availablePower < 0) availablePower = 0;

  // -------- Top status line --------
  display.setTextFont(2);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setCursor(2, 2, 2);
  String clockText = timeClient.getFormattedTime();
  if (clockText.length() >= 5) clockText = clockText.substring(0, 5);
  display.print(clockText);

  display.setCursor(62, 2, 2);
  float waterTemp = gDisplayValues.temperature.toFloat();
  if (waterTemp > 0.0f) {
    display.printf("EAU %.1fC", waterTemp);
  }
  else {
    display.print("EAU --.-C");
  }

  display.setCursor(178, 2, 2);
  if (dimmerFresh) {
    display.setTextColor(TFT_GREEN, TFT_BLACK);
    display.print("CE OK");
  }
  else {
    display.setTextColor(TFT_RED, TFT_BLACK);
    display.print("CE ERR");
  }

  // -------- Main information: power available for another appliance --------
  display.setTextFont(2);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setCursor(96, 23, 2);
  display.print("DISPO");

  int availableColor = TFT_RED;
  if (availablePower >= 2000) availableColor = TFT_GREEN;
  else if (availablePower >= 1000) availableColor = TFT_YELLOW;

  display.setTextFont(4);
  display.setTextColor(availableColor, TFT_BLACK);
  String availableText = gDisplayValues.froniusup
                           ? formatPowerTTGO(availablePower)
                           : String("---");
  display.setCursor(55, 39, 4);
  display.print(availableText);

  // -------- Secondary information --------
  display.setTextFont(2);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setCursor(3, 78, 2);
  display.print("PV ");
  display.setTextColor(TFT_GREEN, TFT_BLACK);
  display.print(formatPowerTTGO((int)gDisplayValues.production));

  display.setCursor(121, 78, 2);
  if (grid < 0) {
    display.setTextColor(TFT_CYAN, TFT_BLACK);
    display.print("EXP ");
    display.print(formatPowerTTGO(-grid));
  }
  else {
    display.setTextColor(TFT_RED, TFT_BLACK);
    display.print("IMP ");
    display.print(formatPowerTTGO(grid));
  }

  display.setCursor(3, 101, 2);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.printf("CE %d%%", reportedDimmer);
  display.setCursor(64, 101, 2);
  display.printf("%dW", heaterPower);

  // -------- Operational status --------
  display.setCursor(118, 101, 2);
  if (!gDisplayValues.froniusup) {
    display.setTextColor(TFT_RED, TFT_BLACK);
    display.print("FRONIUS ERR");
  }
  else if (!dimmerFresh) {
    display.setTextColor(TFT_RED, TFT_BLACK);
    display.print("DIMMER ERR");
  }
  else if (grid > 20) {
    display.setTextColor(TFT_RED, TFT_BLACK);
    display.print("IMPORT");
  }
  else if (reportedDimmer >= 100 && grid < -20) {
    display.setTextColor(TFT_YELLOW, TFT_BLACK);
    display.print("CHAUFFE MAX");
  }
  else if (grid < -20) {
    display.setTextColor(TFT_GREEN, TFT_BLACK);
    display.print("SURPLUS OK");
  }
  else {
    display.setTextColor(TFT_GREEN, TFT_BLACK);
    display.print("ZERO GRID");
  }
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
