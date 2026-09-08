#ifndef TTGO_BOOT_SCREEN
#define TTGO_BOOT_SCREEN

#include <Arduino.h>
#include "../config/version.h"

#ifdef TTGO
#include <TFT_eSPI.h>
extern TFT_eSPI display;

static void bootDrawSun(int x, int y, int color)
{
  display.fillCircle(x, y, 5, color);
  display.drawLine(x, y - 10, x, y - 7, color);
  display.drawLine(x, y + 7, x, y + 10, color);
  display.drawLine(x - 10, y, x - 7, y, color);
  display.drawLine(x + 7, y, x + 10, y, color);
  display.drawLine(x - 7, y - 7, x - 5, y - 5, color);
  display.drawLine(x + 5, y + 5, x + 7, y + 7, color);
  display.drawLine(x + 5, y - 5, x + 7, y - 7, color);
  display.drawLine(x - 7, y + 7, x - 5, y + 5, color);
}

static void bootDrawHouse(int x, int y, int color)
{
  display.drawLine(x - 11, y, x, y - 10, color);
  display.drawLine(x, y - 10, x + 11, y, color);
  display.drawRect(x - 8, y, 16, 13, color);
  display.drawRect(x - 2, y + 6, 4, 7, color);
}

static void bootDrawHeater(int x, int y, int color)
{
  display.drawRoundRect(x - 8, y - 11, 16, 22, 4, color);
  display.drawLine(x - 4, y + 5, x - 1, y - 5, color);
  display.drawLine(x - 1, y - 5, x + 2, y + 5, color);
  display.drawLine(x + 2, y + 5, x + 5, y - 5, color);
}

static void bootDrawGrid(int x, int y, int color)
{
  display.drawLine(x, y - 12, x - 8, y + 12, color);
  display.drawLine(x, y - 12, x + 8, y + 12, color);
  display.drawLine(x - 5, y - 3, x + 5, y - 3, color);
  display.drawLine(x - 7, y + 5, x + 7, y + 5, color);
  display.drawLine(x - 9, y + 12, x + 9, y + 12, color);
}

static void bootDrawFlowArrow(int x1, int x2, int y, int color)
{
  display.drawFastHLine(x1, y, x2 - x1, color);
  display.drawLine(x2, y, x2 - 4, y - 3, color);
  display.drawLine(x2, y, x2 - 4, y + 3, color);
}

// Graphical vector boot screen. It uses only TFT primitives, so it works
// immediately after display.init() and never depends on SPIFFS assets.
static void drawTTGOGraphicalBootScreen(const String &stage,
                                        const String &detail,
                                        uint8_t progress)
{
  if (progress > 100) progress = 100;

  display.fillScreen(TFT_BLACK);
  display.setTextSize(1);

  // Compact identity band.
  display.fillRoundRect(6, 5, 34, 29, 8, TFT_GREEN);
  display.setTextFont(2);
  display.setTextColor(TFT_BLACK, TFT_GREEN);
  display.setCursor(13, 12, 2);
  display.print("PV");

  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setCursor(49, 6, 4);
  display.setTextFont(4);
  display.print("PV ROUTER");
  display.setTextFont(1);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.setCursor(50, 28, 1);
  display.print(String("FW ") + PV_ROUTER_FIRMWARE_LABEL +
                " | ZERO GRID " + PV_ROUTER_ZERO_GRID_LABEL);

  // Energy-flow illustration: sun -> home -> hot-water -> grid.
  const int flowY = 60;
  bootDrawSun(24, flowY, TFT_YELLOW);
  bootDrawFlowArrow(38, 77, flowY, TFT_CYAN);
  bootDrawHouse(96, flowY - 2, progress >= 35 ? TFT_GREEN : TFT_DARKGREY);
  bootDrawFlowArrow(109, 143, flowY, TFT_CYAN);
  bootDrawHeater(160, flowY, progress >= 75 ? TFT_ORANGE : TFT_DARKGREY);
  bootDrawFlowArrow(173, 202, flowY, TFT_CYAN);
  bootDrawGrid(217, flowY, progress >= 90 ? TFT_CYAN : TFT_DARKGREY);

  // A moving energy dot gives each boot stage a slightly animated feel.
  const int dotStart = 39;
  const int dotEnd = 201;
  const int dotX = dotStart + ((dotEnd - dotStart) * progress) / 100;
  display.fillCircle(dotX, flowY, 3, progress >= 100 ? TFT_GREEN : TFT_WHITE);

  // Active stage and optional detail/IP.
  display.setTextFont(2);
  display.setTextColor(progress >= 100 ? TFT_GREEN : TFT_WHITE, TFT_BLACK);
  int stageX = (240 - display.textWidth(stage, 2)) / 2;
  if (stageX < 0) stageX = 0;
  display.setCursor(stageX, 82, 2);
  display.print(stage);

  display.setTextFont(1);
  display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  int detailX = (240 - display.textWidth(detail, 1)) / 2;
  if (detailX < 0) detailX = 0;
  display.setCursor(detailX, 101, 1);
  display.print(detail);

  // Thin progress track with percentage at the right.
  const int barX = 8;
  const int barY = 116;
  const int barW = 190;
  const int barH = 8;
  display.drawRoundRect(barX, barY, barW, barH, 4, TFT_DARKGREY);
  const int fillW = ((barW - 4) * progress) / 100;
  if (fillW > 0) {
    display.fillRoundRect(barX + 2, barY + 2, fillW, barH - 4, 2,
                          progress >= 100 ? TFT_GREEN : TFT_CYAN);
  }

  display.setTextFont(2);
  display.setTextColor(progress >= 100 ? TFT_GREEN : TFT_LIGHTGREY, TFT_BLACK);
  const String pct = String(progress) + "%";
  display.setCursor(205, 112, 2);
  display.print(pct);
}
#endif

#endif
