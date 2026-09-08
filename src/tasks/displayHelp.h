#ifndef TTGO_DISPLAY_HELP
#define TTGO_DISPLAY_HELP

#include <Arduino.h>

#ifdef TTGO
#include <TFT_eSPI.h>
extern TFT_eSPI display;

static void drawHelpRowTTGO(int y,
                            const char *label,
                            int labelColor,
                            const char *description)
{
  display.setTextFont(1);
  display.setTextSize(1);
  display.setTextColor(labelColor, TFT_BLACK);
  display.setCursor(4, y, 1);
  display.print(label);

  display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  display.setCursor(58, y, 1);
  display.print(description);
}

// Static third page intended as an on-device legend. It explains the main
// dashboard without requiring a phone or the Web UI.
static void drawTTGOHelpPage()
{
  display.setTextSize(1);
  display.setTextFont(2);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  const String title = "AIDE AFFICHAGE 3/3";
  int titleX = (240 - display.textWidth(title, 2)) / 2;
  if (titleX < 0) titleX = 0;
  display.setCursor(titleX, 1, 2);
  display.print(title);
  display.drawFastHLine(0, 18, 240, TFT_DARKGREY);

  drawHelpRowTTGO(22,  "T/Tmax",    TFT_GREEN,  "eau ECS / limite thermique");
  drawHelpRowTTGO(31,  "CE OK",     TFT_GREEN,  "dimmer pret et synchronise");
  drawHelpRowTTGO(40,  "TEMP MAX",  TFT_RED,    "temperature limite atteinte");
  drawHelpRowTTGO(49,  "TEMP HOLD", TFT_ORANGE, "attente seuil de reprise");
  drawHelpRowTTGO(58,  "IMPORT",    TFT_RED,    "puissance prise au reseau");
  drawHelpRowTTGO(67,  "DISPO",     TFT_ORANGE, "PV routable, CE deja inclus");
  drawHelpRowTTGO(76,  "SURPLUS",   TFT_CYAN,   "export si CE bloque/sature");
  drawHelpRowTTGO(85,  "PV",        TFT_GREEN,  "production solaire Fronius");
  drawHelpRowTTGO(94,  "EXP/IMP",   TFT_CYAN,   "sens du flux reseau");
  drawHelpRowTTGO(103, "CE % / W",  TFT_ORANGE, "sortie reelle chauffe-eau");
  drawHelpRowTTGO(112, "REPRISE",   TFT_ORANGE, "temperature de redemarrage");

  display.drawFastHLine(0, 122, 240, TFT_DARKGREY);
  display.setTextFont(1);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setCursor(4, 126, 1);
  display.print("Court: page | long: OFF | boot 3s: WiFi");
}
#endif

#endif
