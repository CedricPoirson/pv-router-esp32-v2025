//***********************************
//************* SPIFFS
//***********************************
#ifndef SPIFFS_FUNCTIONS
#define SPIFFS_FUNCTIONS

// File System
#ifdef ESP32
#include <FS.h>
#include "SPIFFS.h"
#include "config/enums.h"
#endif

#include <Wire.h>
#include <ArduinoJson.h>

const char *filename_conf = "/config.json";
extern Config config;

//***********************************
//************* Gestion de la configuration - Lecture du fichier de configuration
//***********************************
void loadConfiguration(const char *filename, Config &config) {
  File configFile = SPIFFS.open(filename_conf, "r");

  StaticJsonDocument<1280> doc;
  DeserializationError error = deserializeJson(doc, configFile);
  if (error) {
    Serial.println(F("Failed to read file, using default configuration in function loadConfiguration"));
  }

  config.port = doc["port"] | 8080;
  strlcpy(config.hostname,
          doc["hostname"] | "192.168.1.20",
          sizeof(config.hostname));
  strlcpy(config.apiKey,
          doc["apiKey"] | "Myapikeystring",
          sizeof(config.apiKey));

  config.UseDomoticz = doc["UseDomoticz"] | false;
  config.UseJeedom = doc["UseJeedom"] | false;
  config.IDX = doc["IDX"] | 100;
  config.IDXdimmer = doc["IDXdimmer"] | 110;

  strlcpy(config.otapassword,
          doc["otapassword"] | "Pvrouteur2",
          sizeof(config.otapassword));

  config.facteur = doc["facteur"] | 0.86;
  config.delta = doc["delta"] | 50;
  config.num_fuse = doc["fuse"] | 70;
  config.deltaneg = doc["deltaneg"] | -100;
  config.cosphi = doc["cosphi"] | 23;
  config.readtime = doc["readtime"] | 555;
  config.cycle = doc["cycle"] | 25;
  config.tmax = doc["tmax"] | 65;
  config.resistance = doc["resistance"] | 1000;
  config.sending = doc["sending"] | true;
  config.autonome = doc["autonome"] | true;
  config.mqtt = doc["mqtt"] | true;
  config.dimmerlocal = doc["dimmerlocal"] | false;
  config.polarity = doc["polarity"] | false;

  strlcpy(config.dimmer,
          doc["dimmer"] | "192.168.1.20",
          sizeof(config.dimmer));
  strlcpy(config.mqttserver,
          doc["mqttserver"] | "192.168.1.20",
          sizeof(config.mqttserver));
  strlcpy(config.Publish,
          doc["Publish"] | "domoticz/in",
          sizeof(config.Publish));

  config.ScreenTime = doc["screentime"] | 0;

  // V14.3 runtime tuning. Existing config.json files do not contain these
  // keys, so they transparently keep the proven controller defaults.
  config.heaterPowerW = doc["heater_power_w"] | 800;
  config.gridTargetW = doc["grid_target_w"] | -15;
  config.gridDeadbandW = doc["grid_deadband_w"] | 10;
  config.dimmerMaxPercent = doc["dimmer_max_percent"] | 100;

  configFile.close();
}

//***********************************
//************* Gestion de la configuration - sauvegarde du fichier de configuration
//***********************************
void saveConfiguration(const char *filename, const Config &config) {
  File configFile = SPIFFS.open(filename_conf, "w");
  if (!configFile) {
    Serial.println(F("Failed to open config file for writing in function Save configuration"));
    return;
  }

  StaticJsonDocument<1280> doc;

  doc["hostname"] = config.hostname;
  doc["port"] = config.port;
  doc["apiKey"] = config.apiKey;
  doc["UseDomoticz"] = config.UseDomoticz;
  doc["UseJeedom"] = config.UseJeedom;
  doc["IDX"] = config.IDX;
  doc["IDXdimmer"] = config.IDXdimmer;
  doc["otapassword"] = config.otapassword;
  doc["delta"] = config.delta;
  doc["deltaneg"] = config.deltaneg;
  doc["cosphi"] = config.cosphi;
  doc["readtime"] = config.readtime;
  doc["cycle"] = config.cycle;
  doc["sending"] = config.sending;
  doc["autonome"] = config.autonome;
  doc["dimmer"] = config.dimmer;
  doc["dimmerlocal"] = config.dimmerlocal;
  doc["facteur"] = config.facteur;
  doc["fuse"] = config.num_fuse;
  doc["mqtt"] = config.mqtt;
  doc["mqttserver"] = config.mqttserver;
  doc["tmax"] = config.tmax;
  doc["resistance"] = config.resistance;
  doc["polarity"] = config.polarity;
  doc["Publish"] = config.Publish;
  doc["screentime"] = config.ScreenTime;

  doc["heater_power_w"] = config.heaterPowerW;
  doc["grid_target_w"] = config.gridTargetW;
  doc["grid_deadband_w"] = config.gridDeadbandW;
  doc["dimmer_max_percent"] = config.dimmerMaxPercent;

  if (serializeJson(doc, configFile) == 0) {
    Serial.println(F("Failed to write to file in function Save configuration"));
  }

  configFile.close();
}

//***********************************
//************* Configuration Wi-Fi SPIFFS
//***********************************
const char *wifi_conf = "/wifi.json";
extern Configwifi configwifi;

void loadwifi(const char *filename, Configwifi &configwifi) {
  File configFile = SPIFFS.open(wifi_conf, "r");

  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, configFile);
  if (error) {
    Serial.println(F("Failed to read wifi config, using config.h fallback"));
  }

  strlcpy(configwifi.SID,
          doc["SID"] | "xxx",
          sizeof(configwifi.SID));
  strlcpy(configwifi.passwd,
          doc["passwd"] | "xxx",
          sizeof(configwifi.passwd));
  configFile.close();
}

// A real SSID stored by the setup portal takes priority over compile-time
// credentials. The historical "xxx" value remains the marker for "not set".
bool hasStoredWifiCredentials(const Configwifi &wifi) {
  return wifi.SID[0] != '\0' && strcmp(wifi.SID, "xxx") != 0;
}

bool savewifi(const char *filename, const Configwifi &wifi) {
  File configFile = SPIFFS.open(wifi_conf, "w");
  if (!configFile) {
    Serial.println(F("Failed to open wifi config for writing"));
    return false;
  }

  StaticJsonDocument<256> doc;
  doc["SID"] = wifi.SID;
  doc["passwd"] = wifi.passwd;

  const bool ok = serializeJson(doc, configFile) > 0;
  configFile.close();

  if (!ok)
    Serial.println(F("Failed to write wifi config"));

  return ok;
}

#endif
