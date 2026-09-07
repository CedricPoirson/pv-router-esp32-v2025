#ifndef PV_ROUTER_WIFI_PROVISIONING
#define PV_ROUTER_WIFI_PROVISIONING

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include "../config/config.h"
#include "../config/enums.h"
#include "../tasks/bootScreen.h"

extern AsyncWebServer server;
extern Configwifi configwifi;
extern bool savewifi(const char *filename, const Configwifi &wifi);

static const char *PVROUTER_SETUP_SSID = "PVRouter-Setup";
static const char *PVROUTER_SETUP_PASSWORD = "pvrouter";
static const unsigned long PVROUTER_SETUP_HOLD_MS = 3000UL;

static const char WIFI_SETUP_PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html lang="fr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>PV Router - Configuration Wi-Fi</title>
<style>
body{margin:0;background:#0b1220;color:#f4f7fb;font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Arial,sans-serif}
main{max-width:540px;margin:auto;padding:28px 18px}
.card{background:#152238;border:1px solid #2b3c58;border-radius:18px;padding:22px;box-shadow:0 18px 45px #0006}
h1{margin:0 0 8px;font-size:24px}.sub{color:#9eb0c9;margin-bottom:22px;line-height:1.45}
label{display:block;font-size:13px;font-weight:700;margin:14px 0 6px}
input{width:100%;box-sizing:border-box;border:1px solid #39506f;background:#0d1728;color:#fff;border-radius:10px;padding:12px;font-size:16px}
button{width:100%;margin-top:20px;border:0;border-radius:11px;padding:13px;background:#45d483;color:#07120c;font-weight:900;font-size:16px}
.info{margin-top:18px;padding:12px;border-radius:10px;background:#0d1728;color:#aebbd0;font-size:13px;line-height:1.45}
code{color:#48c7ef}
</style>
</head>
<body><main><div class="card">
<h1>Configuration Wi-Fi</h1>
<div class="sub">Le PV Router est en mode installation. Saisissez le réseau Wi-Fi de la maison. Après sauvegarde, l'ESP32 redémarrera automatiquement.</div>
<form method="post" action="/save">
<label for="ssid">Nom du Wi-Fi (SSID)</label>
<input id="ssid" name="ssid" maxlength="31" required autocomplete="off">
<label for="passwd">Mot de passe Wi-Fi</label>
<input id="passwd" name="passwd" type="password" maxlength="63" autocomplete="new-password">
<button type="submit">Enregistrer et redémarrer</button>
</form>
<div class="info">Point d'accès temporaire : <code>PVRouter-Setup</code><br>Adresse : <code>192.168.4.1</code><br>Le routage chauffe-eau n'est pas démarré pendant ce mode.</div>
</div></main></body></html>
)HTML";

static const char WIFI_SETUP_SAVED_PAGE[] PROGMEM = R"HTML(
<!doctype html><html lang="fr"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>PV Router</title></head>
<body style="font-family:Arial,sans-serif;background:#0b1220;color:#fff;padding:30px"><h2>Wi-Fi enregistré</h2><p>Le PV Router redémarre. Vous pouvez vous reconnecter à votre réseau Wi-Fi habituel.</p></body></html>
)HTML";

// The configuration portal is deliberately opt-in. It is entered only if the
// physical TTGO button is already held when the unit boots and remains held
// for three seconds. A normal Wi-Fi outage never opens an access point.
static bool wifiProvisioningRequestedAtBoot()
{
#ifdef TTGO
  if (digitalRead(SWITCH) != LOW)
    return false;

  const unsigned long started = millis();
  drawTTGOGraphicalBootScreen("CONFIG WI-FI ?", "Maintenir le bouton 3 s", 20);

  while (digitalRead(SWITCH) == LOW) {
    if ((unsigned long)(millis() - started) >= PVROUTER_SETUP_HOLD_MS)
      return true;
    delay(25);
  }

  drawTTGOGraphicalBootScreen("DEMARRAGE", "Mode normal", 15);
#endif
  return false;
}

#ifdef TTGO
static void drawTTGOWifiProvisioningScreen()
{
  display.fillScreen(TFT_BLACK);
  display.setTextSize(1);

  display.fillRoundRect(6, 5, 34, 29, 8, TFT_CYAN);
  display.setTextFont(2);
  display.setTextColor(TFT_BLACK, TFT_CYAN);
  display.setCursor(13, 12, 2);
  display.print("PV");

  display.setTextFont(4);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setCursor(49, 6, 4);
  display.print("CONFIG WIFI");

  display.drawFastHLine(8, 39, 224, TFT_DARKGREY);

  display.setTextFont(2);
  display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  display.setCursor(10, 48, 2);
  display.print("SSID");
  display.setTextColor(TFT_CYAN, TFT_BLACK);
  display.setCursor(72, 48, 2);
  display.print(PVROUTER_SETUP_SSID);

  display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  display.setCursor(10, 70, 2);
  display.print("PASS");
  display.setTextColor(TFT_YELLOW, TFT_BLACK);
  display.setCursor(72, 70, 2);
  display.print(PVROUTER_SETUP_PASSWORD);

  display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  display.setCursor(10, 92, 2);
  display.print("WEB");
  display.setTextColor(TFT_GREEN, TFT_BLACK);
  display.setCursor(72, 92, 2);
  display.print("192.168.4.1");

  display.setTextFont(1);
  display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  display.setCursor(10, 119, 1);
  display.print("Enregistrer le Wi-Fi puis redemarrage auto");
}
#endif

// Starts a captive local configuration portal and intentionally never returns.
// No Fronius task, dimmer task or MQTT task is started in this mode.
static void runWifiProvisioningMode()
{
  Serial.println(F("[WIFI SETUP] Starting explicit provisioning mode"));

  WiFi.mode(WIFI_AP);
  const IPAddress apIp(192, 168, 4, 1);
  const IPAddress netmask(255, 255, 255, 0);
  WiFi.softAPConfig(apIp, apIp, netmask);

  if (!WiFi.softAP(PVROUTER_SETUP_SSID, PVROUTER_SETUP_PASSWORD)) {
    Serial.println(F("[WIFI SETUP] Failed to start access point"));
#ifdef TTGO
    drawTTGOGraphicalBootScreen("ERREUR WIFI AP", "Redemarrer le PV Router", 0);
#endif
    for (;;) delay(1000);
  }

  DNSServer dnsServer;
  dnsServer.start(53, "*", apIp);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", WIFI_SETUP_PAGE);
  });

  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("ssid", true)) {
      request->send(400, "text/plain", "SSID manquant");
      return;
    }

    String ssid = request->getParam("ssid", true)->value();
    String passwd = request->hasParam("passwd", true)
                        ? request->getParam("passwd", true)->value()
                        : String();
    ssid.trim();

    if (ssid.length() < 1 || ssid.length() > 31) {
      request->send(400, "text/plain", "SSID invalide (1..31 caracteres)");
      return;
    }
    if (passwd.length() > 63) {
      request->send(400, "text/plain", "Mot de passe Wi-Fi trop long");
      return;
    }

    Configwifi nextWifi = {};
    strlcpy(nextWifi.SID, ssid.c_str(), sizeof(nextWifi.SID));
    strlcpy(nextWifi.passwd, passwd.c_str(), sizeof(nextWifi.passwd));

    if (!savewifi(wifi_conf, nextWifi)) {
      request->send(500, "text/plain", "Impossible d'enregistrer wifi.json");
      return;
    }

    configwifi = nextWifi;
    Serial.printf("[WIFI SETUP] Saved SSID '%s'; restarting\n", configwifi.SID);
    request->send_P(200, "text/html", WIFI_SETUP_SAVED_PAGE);
    delay(800);
    ESP.restart();
  });

  // Common captive-portal probes. Any unknown URL is redirected to the setup
  // form, so iOS/Android/Windows usually open the portal automatically.
  server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->redirect("http://192.168.4.1/");
  });
  server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->redirect("http://192.168.4.1/");
  });
  server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->redirect("http://192.168.4.1/");
  });
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->redirect("http://192.168.4.1/");
  });

  server.begin();

#ifdef TTGO
  drawTTGOWifiProvisioningScreen();
#endif

  Serial.printf("[WIFI SETUP] AP=%s PASS=%s IP=%s\n",
                PVROUTER_SETUP_SSID,
                PVROUTER_SETUP_PASSWORD,
                WiFi.softAPIP().toString().c_str());

  for (;;) {
    dnsServer.processNextRequest();
    delay(10);
  }
}

#endif
