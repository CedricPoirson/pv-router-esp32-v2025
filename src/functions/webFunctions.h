#ifdef ESP32
#include <AsyncTCP.h>
#elif defined(ESP8266)
#include <ESPAsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include "appweb.h"
#include "froniusZeroGrid.h"

extern DisplayValues gDisplayValues;
extern PubSubClient client;

//***********************************
//************* Gestion du serveur WEB
//***********************************

String inputMessage;
AsyncWebServer server(80);

static String buildApiStatus()
{
  const unsigned long now = millis();

  const bool dimmerOnline =
      gDisplayValues.dimmerCommOk &&
      gDisplayValues.dimmerLastOkMs > 0 &&
      ((unsigned long)(now - gDisplayValues.dimmerLastOkMs) <= 15000UL);

  int dimmerCmd = constrain(gDisplayValues.dimmer, 0, 100);
  int dimmerActual = dimmerOnline ? constrain(gDisplayValues.dimmerReported, 0, 100) : 0;

  const int heaterPower = (FRONIUS_HEATER_POWER_W * dimmerActual) / 100;
  const int gridPower = (int)gDisplayValues.grid;
  const int pvPower = (int)gDisplayValues.production;

  int housePower = pvPower + gridPower;
  if (housePower < 0) housePower = 0;

  int availablePower = heaterPower - gridPower;
  if (availablePower < 0) availablePower = 0;

  const float waterTemp = gDisplayValues.temperature.toFloat();
  const int remoteMaxTemp = gDisplayValues.dimmerMaxTemp;
  const int effectiveMaxTemp = remoteMaxTemp > 0 ? remoteMaxTemp : config.tmax;

  const bool dimmerSynced =
      dimmerOnline && (abs(dimmerCmd - dimmerActual) <= 2);

  const bool tempAtMax =
      waterTemp > 0.0f &&
      effectiveMaxTemp > 0 &&
      waterTemp >= (float)effectiveMaxTemp;

  String status;
  if (!gDisplayValues.froniusup)
    status = "FRONIUS OFFLINE";
  else if (!dimmerOnline)
    status = "DIMMER OFFLINE";
  else if (tempAtMax)
    status = "TEMP MAX";
  else if (gDisplayValues.dimmerAlarm)
    status = "DIMMER ALARM";
  else if (!dimmerSynced)
    status = "CE SYNC";
  else if (dimmerActual >= 100 && gridPower < -20)
    status = "LOAD LIMITED";
  else if (gridPower > 20)
    status = "IMPORT";
  else if (gridPower < -20)
    status = "SURPLUS";
  else
    status = "ZERO GRID";

  StaticJsonDocument<2304> doc;
  doc["version"] = String(VERSION);
  doc["uptime_s"] = now / 1000UL;
  doc["ip"] = gDisplayValues.IP;
  doc["wifi_rssi"] = WiFi.isConnected() ? WiFi.RSSI() : -127;
  doc["wifi_ssid"] = WiFi.isConnected() ? WiFi.SSID() : "";
  doc["heap_free"] = ESP.getFreeHeap();
  doc["mqtt_connected"] = client.connected();
  doc["mqtt_state"] = client.state();
  doc["status"] = status;

  doc["pv_w"] = pvPower;
  doc["grid_w"] = gridPower;
  doc["house_w"] = housePower;
  doc["available_w"] = availablePower;

  doc["fronius"] = gDisplayValues.froniusup;
  if (gDisplayValues.froniusLastOkMs > 0)
    doc["fronius_age_ms"] = (unsigned long)(now - gDisplayValues.froniusLastOkMs);
  else
    doc["fronius_age_ms"] = nullptr;

  JsonObject regulation = doc.createNestedObject("regulation");
  regulation["version"] = "V14.3";
  regulation["target_w"] = FRONIUS_GRID_TARGET_W;
  regulation["low_w"] = FRONIUS_GRID_TARGET_W - FRONIUS_GRID_DEADBAND_W;
  regulation["high_w"] = FRONIUS_GRID_TARGET_W + FRONIUS_GRID_DEADBAND_W;
  regulation["correction_w"] = FRONIUS_GRID_TARGET_W - gridPower;
  regulation["heater_model_w"] = FRONIUS_HEATER_POWER_W;
  regulation["state"] = status;

  JsonObject dimmer = doc.createNestedObject("dimmer");
  dimmer["online"] = dimmerOnline;
  dimmer["synced"] = dimmerSynced;
  dimmer["cmd"] = dimmerCmd;
  dimmer["actual"] = dimmerActual;
  dimmer["heater_w"] = heaterPower;
  dimmer["rssi"] = gDisplayValues.dimmerRssi;
  dimmer["on"] = gDisplayValues.dimmerOn;
  dimmer["alarm"] = gDisplayValues.dimmerAlarm;
  dimmer["alert"] = gDisplayValues.dimmerAlert;
  dimmer["version"] = gDisplayValues.dimmerVersion;
  if (gDisplayValues.dimmerLastOkMs > 0)
    dimmer["age_ms"] = (unsigned long)(now - gDisplayValues.dimmerLastOkMs);
  else
    dimmer["age_ms"] = nullptr;

  JsonObject ecs = doc.createNestedObject("ecs");
  if (waterTemp > 0.0f)
    ecs["temp_c"] = waterTemp;
  else
    ecs["temp_c"] = nullptr;
  ecs["max_c"] = effectiveMaxTemp;
  ecs["remote_max_c"] = remoteMaxTemp > 0 ? remoteMaxTemp : 0;
  ecs["fallback_max_c"] = config.tmax;
  ecs["at_max"] = tempAtMax;

  String payload;
  serializeJson(doc, payload);
  return payload;
}

static String buildApiConfig(bool saved = false)
{
  StaticJsonDocument<768> doc;
  doc["saved"] = saved;
  doc["dimmer_ip"] = config.dimmer;
  doc["screen_timeout_s"] = config.ScreenTime;
  doc["fallback_tmax_c"] = config.tmax;
  doc["heater_power_w"] = FRONIUS_HEATER_POWER_W;
  doc["autonomous"] = config.autonome;
  doc["mqtt_runtime_editable"] = false;

  String payload;
  serializeJson(doc, payload);
  return payload;
}

static void sendJsonError(AsyncWebServerRequest *request,
                          int code,
                          const String &message)
{
  StaticJsonDocument<256> doc;
  doc["ok"] = false;
  doc["error"] = message;
  String payload;
  serializeJson(doc, payload);
  request->send(code, "application/json", payload);
}

void notFound(AsyncWebServerRequest *request)
{
  request->send(404, "text/plain", "Not found");
}

void call_pages()
{
  // V2 local UI. index.html and config.html are self-contained and do not
  // require Internet access, jQuery, Bootstrap or Google Charts.
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (SPIFFS.exists("/index.html"))
      request->send(SPIFFS, "/index.html", "text/html");
    else
      request->send_P(200, "text/plain", SPIFFSNO);
  });

  server.on("/config.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (SPIFFS.exists("/config.html"))
      request->send(SPIFFS, "/config.html", "text/html");
    else
      request->send(404, "text/plain", "config.html missing");
  });

  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/favicon.ico", "image/png");
  });

  // ---------------- V2 JSON API ----------------
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", buildApiStatus());
  });

  server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", buildApiConfig());
  });

  server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool changed = false;

    if (request->hasParam("dimmer_ip", true)) {
      const String value = request->getParam("dimmer_ip", true)->value();
      IPAddress ip;
      if (!ip.fromString(value)) {
        sendJsonError(request, 400, "Adresse IP RobotDyn invalide");
        return;
      }
      if (value.length() >= sizeof(config.dimmer)) {
        sendJsonError(request, 400, "Adresse IP RobotDyn trop longue");
        return;
      }
      strlcpy(config.dimmer, value.c_str(), sizeof(config.dimmer));
      changed = true;
    }

    if (request->hasParam("screen_timeout_s", true)) {
      const int value = request->getParam("screen_timeout_s", true)->value().toInt();
      if (value < 0 || value > 86400) {
        sendJsonError(request, 400, "Timeout ecran hors plage 0..86400 s");
        return;
      }
      config.ScreenTime = value;
      changed = true;
    }

    if (request->hasParam("fallback_tmax_c", true)) {
      const int value = request->getParam("fallback_tmax_c", true)->value().toInt();
      if (value < 1 || value > 90) {
        sendJsonError(request, 400, "Tmax secours hors plage 1..90 C");
        return;
      }
      config.tmax = value;
      changed = true;
    }

    if (changed) {
      Serial.println(F("[WEB] Saving V2 configuration..."));
      saveConfiguration(filename_conf, config);
    }

    request->send(200, "application/json", buildApiConfig(changed));
  });

  // Download the complete SPIFFS configuration so it can be restored before
  // or after an uploadfs operation. wifi.json deliberately remains separate.
  server.on("/api/config/export", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!SPIFFS.exists(filename_conf)) {
      sendJsonError(request, 404, "config.json introuvable");
      return;
    }
    request->send(SPIFFS, filename_conf, "application/json", true);
  });

  // Restore a complete config.json sent by the local V2 page. The JSON is
  // validated before replacing the current file, then applied immediately.
  server.on("/api/config/import", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("config_json", true)) {
      sendJsonError(request, 400, "Fichier de configuration absent");
      return;
    }

    const String raw = request->getParam("config_json", true)->value();
    if (raw.length() == 0 || raw.length() > 4096) {
      sendJsonError(request, 400, "Taille de configuration invalide");
      return;
    }

    StaticJsonDocument<2048> imported;
    const DeserializationError error = deserializeJson(imported, raw);
    if (error || !imported.is<JsonObject>()) {
      sendJsonError(request, 400, "JSON de configuration invalide");
      return;
    }

    // Require the two keys that identify a current PV Router config and avoid
    // silently replacing it with an unrelated but syntactically valid JSON.
    if (!imported.containsKey("dimmer") || !imported.containsKey("screentime")) {
      sendJsonError(request, 400, "Ce fichier ne ressemble pas a une configuration PV Router");
      return;
    }

    File configFile = SPIFFS.open(filename_conf, "w");
    if (!configFile) {
      sendJsonError(request, 500, "Impossible d'ouvrir config.json en ecriture");
      return;
    }

    const size_t written = configFile.print(raw);
    configFile.close();
    if (written != raw.length()) {
      sendJsonError(request, 500, "Ecriture incomplete de config.json");
      return;
    }

    loadConfiguration(filename_conf, config);
    Serial.println(F("[WEB] Configuration restored from V2 upload"));
    request->send(200,
                  "application/json",
                  "{\"ok\":true,\"restored\":true,\"restart_recommended\":true}");
  });

  server.on("/api/screen/toggle", HTTP_POST, [](AsyncWebServerRequest *request) {
    gDisplayValues.screenstate = HIGH;
    request->send(200, "application/json", "{\"ok\":true}");
  });

  server.on("/api/restart", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", "{\"ok\":true,\"restarting\":true}");
    delay(250);
    ESP.restart();
  });

  // ---------------- Legacy endpoints ----------------
  // Kept temporarily for compatibility with older tooling while the V2 UI is
  // rolled out. The new pages use only /api/*.
  server.on("/all.min.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/all.min.css", "text/css");
  });
  server.on("/fa-solid-900.woff2", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/fa-solid-900.woff2", "font/woff2");
  });
  server.on("/sb-admin-2.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/sb-admin-2.js", "text/javascript");
  });
  server.on("/sb-admin-2.min.css", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/sb-admin-2.min.css", "text/css");
  });
  server.on("/chart.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "application/json", getchart().c_str());
  });
  server.on("/sendmode", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/plain", getSendmode().c_str());
  });
  server.on("/state", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/plain", getState().c_str());
  });
  server.on("/serial", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/plain", getState().c_str());
  });
  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/plain", getconfig().c_str());
  });
  server.on("/memory", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/plain", getmemory().c_str());
  });
  server.on("/debug", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/plain", getdebug().c_str());
  });
  server.on("/log", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/plain", getlogs().c_str());
  });
  server.on("/config.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/config.json", "application/json");
  });
  server.on("/doc.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/doc.txt", "text/plain");
  });
  server.on("/cosphi", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/plain", getcosphi().c_str());
  });
  server.on("/puissance", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/plain", getpuissance().c_str());
  });

  server.on("/get", HTTP_ANY, [](AsyncWebServerRequest *request) {
    if (request->hasParam(PARAM_INPUT_1)) {
      inputMessage = request->getParam(PARAM_INPUT_1)->value();
      config.sending = 0;
      if (inputMessage != "On") config.sending = 1;
    }
    if (request->hasParam(PARAM_INPUT_save)) {
      Serial.println(F("Saving configuration..."));
      saveConfiguration(filename_conf, config);
    }
    if (request->hasParam(PARAM_INPUT_2)) config.cycle = request->getParam(PARAM_INPUT_2)->value().toInt();
    if (request->hasParam(PARAM_INPUT_3)) config.readtime = request->getParam(PARAM_INPUT_3)->value().toInt();
    if (request->hasParam(PARAM_INPUT_4)) config.cosphi = request->getParam(PARAM_INPUT_4)->value().toInt();
    if (request->hasParam(PARAM_INPUT_dimmer)) request->getParam(PARAM_INPUT_dimmer)->value().toCharArray(config.dimmer, sizeof(config.dimmer));
    if (request->hasParam(PARAM_INPUT_server)) request->getParam(PARAM_INPUT_server)->value().toCharArray(config.hostname, sizeof(config.hostname));
    if (request->hasParam(PARAM_INPUT_mqttserver)) request->getParam(PARAM_INPUT_mqttserver)->value().toCharArray(config.mqttserver, sizeof(config.mqttserver));
    if (request->hasParam(PARAM_INPUT_publish)) request->getParam(PARAM_INPUT_publish)->value().toCharArray(config.Publish, sizeof(config.Publish));
    if (request->hasParam(PARAM_INPUT_delta)) config.delta = request->getParam(PARAM_INPUT_delta)->value().toInt();
    if (request->hasParam(PARAM_INPUT_deltaneg)) config.deltaneg = request->getParam(PARAM_INPUT_deltaneg)->value().toInt();
    if (request->hasParam(PARAM_INPUT_fuse)) config.num_fuse = request->getParam(PARAM_INPUT_fuse)->value().toInt();
    if (request->hasParam(PARAM_INPUT_port)) config.port = request->getParam(PARAM_INPUT_port)->value().toInt();
    if (request->hasParam(PARAM_INPUT_IDX)) config.IDX = request->getParam(PARAM_INPUT_IDX)->value().toInt();
    if (request->hasParam(PARAM_INPUT_IDXdimmer)) config.IDXdimmer = request->getParam(PARAM_INPUT_IDXdimmer)->value().toInt();
    if (request->hasParam(PARAM_INPUT_API)) request->getParam(PARAM_INPUT_API)->value().toCharArray(config.apiKey, sizeof(config.apiKey));
    if (request->hasParam(PARAM_INPUT_dimmer_power)) {
      gDisplayValues.dimmer = request->getParam(PARAM_INPUT_dimmer_power)->value().toInt();
      gDisplayValues.change = 1;
    }
    if (request->hasParam(PARAM_INPUT_facteur)) config.facteur = request->getParam(PARAM_INPUT_facteur)->value().toFloat();
    if (request->hasParam(PARAM_INPUT_tmax)) config.tmax = request->getParam(PARAM_INPUT_tmax)->value().toInt();
    if (request->hasParam("resistance")) config.resistance = request->getParam("resistance")->value().toInt();
    if (request->hasParam("screentime")) config.ScreenTime = request->getParam("screentime")->value().toInt();
    if (request->hasParam(PARAM_INPUT_reset)) {
      request->send(200, "text/plain", "Restarting");
      delay(250);
      ESP.restart();
      return;
    }
    if (request->hasParam(PARAM_INPUT_servermode)) {
      inputMessage = request->getParam(PARAM_INPUT_servermode)->value();
      getServermode(inputMessage);
    }

    request->send(200, "text/plain", getconfig().c_str());
  });

  server.onNotFound(notFound);
}
