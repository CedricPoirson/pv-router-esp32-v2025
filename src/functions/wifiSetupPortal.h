#ifndef WIFI_SETUP_PORTAL_H
#define WIFI_SETUP_PORTAL_H

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include "config/config.h"
#include "config/enums.h"

#ifdef TTGO
#include <TFT_eSPI.h>
extern TFT_eSPI display;
#endif

extern Configwifi configwifi;
extern const char *wifi_conf;

static const char *PVROUTER_SETUP_SSID = "PVRouter-Setup";
static const char *PVROUTER_SETUP_PASSWORD = "pvrouter14";
static const unsigned long PVROUTER_SETUP_HOLD_MS = 3000UL;

static DNSServer gSetupDnsServer;
static AsyncWebServer gSetupWebServer(80);
static volatile bool gSetupRestartRequested = false;
static unsigned long gSetupRestartAt = 0;
static String gSetupNetworkOptions;

static String wifiSetupHtmlEscape(const String &value)
{
  String out;
  out.reserve(value.length() + 8);
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value[i];
    switch (c) {
      case '&': out += F("&amp;"); break;
      case '<': out += F("&lt;"); break;
      case '>': out += F("&gt;"); break;
      case '"': out += F("&quot;"); break;
      case '\'': out += F("&#39;"); break;
      default: out += c; break;
    }
  }
  return out;
}

#ifdef TTGO
static void drawWifiSetupHoldScreen(unsigned long elapsedMs)
{
  display.fillScreen(TFT_BLACK);
  display.setTextFont(2);
  display.setTextSize(1);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.setCursor(35, 12, 2);
  display.print("CONFIGURATION WI-FI");

  display.setTextFont(1);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setCursor(42, 49, 1);
  display.print("Maintenir le bouton 3 s");

  display.drawRoundRect(20, 76, 200, 15, 5, TFT_DARKGREY);
  const int fill = constrain((int)((elapsedMs * 196UL) / PVROUTER_SETUP_HOLD_MS), 0, 196);
  if (fill > 0)
    display.fillRoundRect(22, 78, fill, 11, 4, TFT_CYAN);

  display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  display.setCursor(57, 106, 1);
  display.print("Relacher = demarrage normal");
}

static void drawWifiSetupPortalScreen()
{
  display.fillScreen(TFT_BLACK);
  display.setTextFont(2);
  display.setTextSize(1);
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.setCursor(55, 2, 2);
  display.print("MODE CONFIG WIFI");
  display.drawFastHLine(0, 21, 240, TFT_DARKGREY);

  display.setTextFont(1);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setCursor(8, 31, 1);
  display.print("Wi-Fi : ");
  display.setTextColor(TFT_GREEN, TFT_BLACK);
  display.print(PVROUTER_SETUP_SSID);

  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setCursor(8, 47, 1);
  display.print("Mot de passe : ");
  display.setTextColor(TFT_ORANGE, TFT_BLACK);
  display.print(PVROUTER_SETUP_PASSWORD);

  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setCursor(8, 68, 1);
  display.print("Puis ouvrir :");
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.setCursor(8, 82, 1);
  display.print("http://192.168.4.1");

  display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  display.setCursor(8, 106, 1);
  display.print("Le routeur redemarre apres sauvegarde.");
}

static void drawWifiSetupSavedScreen(const String &ssid)
{
  display.fillScreen(TFT_BLACK);
  display.setTextFont(2);
  display.setTextColor(TFT_GREEN, TFT_BLACK);
  display.setCursor(48, 18, 2);
  display.print("WI-FI ENREGISTRE");

  display.setTextFont(1);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setCursor(12, 59, 1);
  display.print("Reseau : ");
  display.print(ssid.substring(0, 27));

  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.setCursor(50, 91, 1);
  display.print("Redemarrage du PV Router...");
}
#endif

// Enter setup only when the existing TTGO button is already held during boot
// and remains held for three seconds. A normal reboot therefore never exposes
// the setup access point by itself.
static bool wifiSetupRequestedAtBoot()
{
#ifdef TTGO
  if (digitalRead(SWITCH) != LOW)
    return false;

  const unsigned long started = millis();
  unsigned long lastDraw = 0;

  while (digitalRead(SWITCH) == LOW) {
    const unsigned long elapsed = millis() - started;
    if (elapsed - lastDraw >= 100UL || lastDraw == 0) {
      drawWifiSetupHoldScreen(elapsed);
      lastDraw = elapsed;
    }

    if (elapsed >= PVROUTER_SETUP_HOLD_MS)
      return true;

    delay(20);
  }
#endif
  return false;
}

static String buildWifiSetupPage()
{
  String page;
  page.reserve(6500 + gSetupNetworkOptions.length());
  page += F("<!doctype html><html lang='fr'><head><meta charset='utf-8'>");
  page += F("<meta name='viewport' content='width=device-width,initial-scale=1'>");
  page += F("<meta name='theme-color' content='#0b1220'><title>PV Router - Wi-Fi</title>");
  page += F("<style>*{box-sizing:border-box}body{margin:0;background:#0b1220;color:#f4f7fb;font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Arial,sans-serif}.wrap{max-width:560px;margin:auto;padding:24px 18px}.logo{width:58px;height:58px;border-radius:18px;background:linear-gradient(145deg,#42e686,#48c7ef);display:grid;place-items:center;color:#07130d;font-size:26px;font-weight:900;margin-bottom:18px}h1{font-size:25px;margin:0 0 8px}p{color:#9babc1;line-height:1.5}.card{background:#142035;border:1px solid #273953;border-radius:18px;padding:20px;margin-top:20px}label{display:block;font-size:13px;font-weight:750;margin:15px 0 7px}input{width:100%;padding:13px;border-radius:11px;border:1px solid #344967;background:#0c1627;color:#fff;font-size:16px}button{width:100%;margin-top:22px;padding:14px;border:0;border-radius:12px;background:#45d483;color:#07120c;font-size:16px;font-weight:850}.hint{font-size:12px;color:#7f91aa}.badge{display:inline-block;padding:5px 9px;border-radius:999px;background:#20324d;color:#48c7ef;font-size:12px;font-weight:750}</style></head><body><div class='wrap'>");
  page += F("<div class='logo'>PV</div><span class='badge'>MODE CONFIGURATION</span><h1>Configurer le Wi-Fi</h1>");
  page += F("<p>Choisissez le réseau Wi-Fi de la maison. Après sauvegarde, le PV Router redémarrera et utilisera ces identifiants en priorité.</p>");
  page += F("<div class='card'><form method='post' action='/save'>");
  page += F("<label>Nom du réseau / Wi-Fi SSID</label><input name='ssid' list='networks' maxlength='31' required autocomplete='off' placeholder='Mon Wi-Fi'><datalist id='networks'>");
  page += gSetupNetworkOptions;
  page += F("</datalist><label>Mot de passe / Password</label><input name='password' type='password' maxlength='63' autocomplete='new-password' placeholder='Mot de passe Wi-Fi'>");
  page += F("<div class='hint'>Laisser vide uniquement pour un réseau Wi-Fi ouvert.</div><button type='submit'>Enregistrer et redémarrer</button></form></div>");
  page += F("<p class='hint'>Ce point d'accès n'apparaît que lorsque le bouton du TTGO est maintenu pendant 3 secondes au démarrage.</p></div></body></html>");
  return page;
}

static void buildWifiScanOptions()
{
  gSetupNetworkOptions = "";
  const int count = WiFi.scanNetworks(false, true);
  if (count <= 0)
    return;

  for (int i = 0; i < count; ++i) {
    const String ssid = WiFi.SSID(i);
    if (ssid.length() == 0)
      continue;

    const String escaped = wifiSetupHtmlEscape(ssid);
    if (gSetupNetworkOptions.indexOf("value='" + escaped + "'") >= 0)
      continue;

    gSetupNetworkOptions += F("<option value='");
    gSetupNetworkOptions += escaped;
    gSetupNetworkOptions += F("'>");
  }
  WiFi.scanDelete();
}

// Blocking by design: setup mode is a dedicated maintenance mode. Normal
// regulation tasks are not started until credentials have been saved and the
// ESP32 has rebooted.
static void runWifiSetupPortal()
{
  Serial.println(F("[WIFI-SETUP] Entering physical-button setup mode"));

  WiFi.mode(WIFI_AP_STA);
  delay(100);

  if (!WiFi.softAP(PVROUTER_SETUP_SSID, PVROUTER_SETUP_PASSWORD)) {
    Serial.println(F("[WIFI-SETUP] Failed to start access point"));
#ifdef TTGO
    display.fillScreen(TFT_BLACK);
    display.setTextColor(TFT_RED, TFT_BLACK);
    display.setTextFont(2);
    display.setCursor(30, 45, 2);
    display.print("ERREUR POINT D'ACCES");
#endif
    for (;;) delay(1000);
  }

  buildWifiScanOptions();

  const IPAddress apIp = WiFi.softAPIP();
  Serial.printf("[WIFI-SETUP] AP=%s IP=%s\n",
                PVROUTER_SETUP_SSID,
                apIp.toString().c_str());

#ifdef TTGO
  drawWifiSetupPortalScreen();
#endif

  gSetupDnsServer.start(53, "*", apIp);

  gSetupWebServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html; charset=utf-8", buildWifiSetupPage());
  });

  gSetupWebServer.on("/save", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("ssid", true)) {
      request->send(400, "text/plain; charset=utf-8", "SSID manquant / Missing SSID");
      return;
    }

    String ssid = request->getParam("ssid", true)->value();
    String password = request->hasParam("password", true)
                        ? request->getParam("password", true)->value()
                        : String();
    ssid.trim();

    if (ssid.length() == 0 || ssid.length() >= sizeof(configwifi.SID) ||
        password.length() >= sizeof(configwifi.passwd)) {
      request->send(400, "text/plain; charset=utf-8", "Parametres Wi-Fi invalides / Invalid Wi-Fi settings");
      return;
    }

    strlcpy(configwifi.SID, ssid.c_str(), sizeof(configwifi.SID));
    strlcpy(configwifi.passwd, password.c_str(), sizeof(configwifi.passwd));

    if (!savewifi(wifi_conf, configwifi)) {
      request->send(500, "text/plain; charset=utf-8", "Echec sauvegarde SPIFFS / SPIFFS save failed");
      return;
    }

    Serial.printf("[WIFI-SETUP] Saved SSID '%s'\n", configwifi.SID);
#ifdef TTGO
    drawWifiSetupSavedScreen(ssid);
#endif

    request->send(200,
                  "text/html; charset=utf-8",
                  "<!doctype html><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><style>body{background:#0b1220;color:#fff;font-family:sans-serif;padding:30px;text-align:center}h1{color:#45d483}</style><h1>Wi-Fi enregistre</h1><p>Le PV Router redemarre...</p><p>Wi-Fi saved. PV Router is restarting...</p>");
    gSetupRestartRequested = true;
    gSetupRestartAt = millis() + 1800UL;
  });

  // Common captive-portal probes and every unknown URL come back to the
  // setup form, making phone onboarding less dependent on the browser used.
  gSetupWebServer.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->redirect("http://192.168.4.1/");
  });
  gSetupWebServer.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->redirect("http://192.168.4.1/");
  });
  gSetupWebServer.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->redirect("http://192.168.4.1/");
  });
  gSetupWebServer.onNotFound([](AsyncWebServerRequest *request) {
    request->redirect("http://192.168.4.1/");
  });

  gSetupWebServer.begin();

  for (;;) {
    gSetupDnsServer.processNextRequest();
    if (gSetupRestartRequested && (long)(millis() - gSetupRestartAt) >= 0) {
      delay(50);
      ESP.restart();
    }
    delay(10);
  }
}

#endif
