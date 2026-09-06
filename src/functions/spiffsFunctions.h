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

#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include <ArduinoJson.h> // ArduinoJson : https://github.com/bblanchon/ArduinoJson



const char *filename_conf = "/config.json";
extern Config config;

//***********************************
//************* Gestion de la configuration - Lecture du fichier de configuration
//***********************************
// Loads the configuration from a file
void loadConfiguration(const char *filename, Config &config) {
  // Open file for reading
  File configFile = SPIFFS.open(filename_conf, "r");

  // Allocate a temporary JsonDocument
  StaticJsonDocument<1024> doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, configFile);
  if (error) {
    Serial.println(F("Failed to read file, using default configuration in function loadConfiguration"));
  }

  // Copy values from the JsonDocument to the Config
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
  config.num_fuse = doc["fuse"] | 100;
  config.deltaneg = doc["deltaneg"] | -100;
  config.cosphi = doc["cosphi"] | 23;
  config.readtime = doc["readtime"] | 555;
  config.cycle = doc["cycle"] | 25;
  config.tmax = doc["tmax"] | 65;
  config.resistance = doc["resistance"] | 800;
  config.sending = doc["sending"] | true;
  config.autonome = doc["autonome"] | true;
  config.mqtt = doc["mqtt"] | true;
  config.dimmerlocal = doc["dimmerlocal"] | false;
  config.polarity = doc["polarity"] | false;

  strlcpy(config.dimmer,
          doc["dimmer"] | "192.168.100.29",
          sizeof(config.dimmer));

  strlcpy(config.mqttserver,
          doc["mqttserver"] | "192.168.1.20",
          sizeof(config.mqttserver));

  strlcpy(config.Publish,
          doc["Publish"] | "domoticz/in",
          sizeof(config.Publish));

  config.ScreenTime = doc["screentime"] | 0;
  configFile.close();
}

//***********************************
//************* Gestion de la configuration - sauvegarde du fichier de configuration
//***********************************

void saveConfiguration(const char *filename, const Config &config) {
  // Open file for writing
  File configFile = SPIFFS.open(filename_conf, "w");
  if (!configFile) {
    Serial.println(F("Failed to open config file for writing in function Save configuration"));
    return;
  }

  StaticJsonDocument<1024> doc;

  // Set the values in the document
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

  if (serializeJson(doc, configFile) == 0) {
    Serial.println(F("Failed to write to file in function Save configuration "));
  }

  configFile.close();
}

///// config Wifi

const char *wifi_conf = "/wifi.json";
extern Configwifi configwifi;

void loadwifi(const char *filename, Configwifi &configwifi) {
  File configFile = SPIFFS.open(wifi_conf, "r");

  StaticJsonDocument<512> doc;

  DeserializationError error = deserializeJson(doc, configFile);
  if (error) {
    Serial.println(F("Failed to read wifi config, using default configuration in function config.h"));
  }

  strlcpy(configwifi.SID,
          doc["SID"] | "xxx",
          sizeof(configwifi.SID));

  strlcpy(configwifi.passwd,
          doc["passwd"] | "xxx",
          sizeof(configwifi.passwd));
  configFile.close();
}

#endif
