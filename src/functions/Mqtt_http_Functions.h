#ifndef MQTT_FUNCTIONS
#define MQTT_FUNCTIONS

#include <Arduino.h>
#include "../config/config.h"
#include "../config/enums.h"
#include <PubSubClient.h>
#include <WiFi.h>

WiFiClient espClient;
PubSubClient client(espClient);
extern DisplayValues gDisplayValues;
extern Config config;

static const char* PVROUTER_STATE_TOPIC = "pvrouter/state";
static const char* PVROUTER_AVAILABILITY_TOPIC = "pvrouter/availability";

void Mqtt_send(String sensor, String value);
void Mqtt_publishState();

static String mqttDeviceBlock()
{
  String device = "\"device\":{";
  device += "\"identifiers\":[\"pvrouter_esp32\"],";
  device += "\"name\":\"PVRouter ESP32\",";
  device += "\"manufacturer\":\"Cédric Poirson\",";
  device += "\"model\":\"TTGO T-Display / PV Router\"";
  device += "}";
  return device;
}

static void publishSensorDiscovery(const char* objectId,
                                   const char* name,
                                   const char* valueKey,
                                   const char* unit,
                                   const char* deviceClass,
                                   const char* stateClass,
                                   const char* icon,
                                   bool diagnostic = false)
{
  String topic = "homeassistant/sensor/" + String(objectId) + "/config";

  String payload;
  payload.reserve(700);
  payload = "{";
  payload += "\"name\":\"" + String(name) + "\",";
  payload += "\"unique_id\":\"" + String(objectId) + "_esp32\",";
  payload += "\"state_topic\":\"" + String(PVROUTER_STATE_TOPIC) + "\",";
  payload += "\"value_template\":\"{{ value_json." + String(valueKey) + " }}\",";
  payload += "\"availability_topic\":\"" + String(PVROUTER_AVAILABILITY_TOPIC) + "\",";
  payload += "\"payload_available\":\"online\",";
  payload += "\"payload_not_available\":\"offline\",";

  if (unit && unit[0]) {
    payload += "\"unit_of_measurement\":\"" + String(unit) + "\",";
  }
  if (deviceClass && deviceClass[0]) {
    payload += "\"device_class\":\"" + String(deviceClass) + "\",";
  }
  if (stateClass && stateClass[0]) {
    payload += "\"state_class\":\"" + String(stateClass) + "\",";
  }
  if (icon && icon[0]) {
    payload += "\"icon\":\"" + String(icon) + "\",";
  }
  if (diagnostic) {
    payload += "\"entity_category\":\"diagnostic\",";
  }

  payload += mqttDeviceBlock();
  payload += "}";

  client.publish(topic.c_str(), payload.c_str(), true);
}

static void publishBinarySensorDiscovery(const char* objectId,
                                         const char* name,
                                         const char* valueKey,
                                         const char* deviceClass,
                                         const char* icon,
                                         bool diagnostic = false)
{
  String topic = "homeassistant/binary_sensor/" + String(objectId) + "/config";

  String payload;
  payload.reserve(750);
  payload = "{";
  payload += "\"name\":\"" + String(name) + "\",";
  payload += "\"unique_id\":\"" + String(objectId) + "_esp32\",";
  payload += "\"state_topic\":\"" + String(PVROUTER_STATE_TOPIC) + "\",";
  payload += "\"value_template\":\"{{ 'ON' if value_json." + String(valueKey) + " else 'OFF' }}\",";
  payload += "\"payload_on\":\"ON\",";
  payload += "\"payload_off\":\"OFF\",";
  payload += "\"availability_topic\":\"" + String(PVROUTER_AVAILABILITY_TOPIC) + "\",";
  payload += "\"payload_available\":\"online\",";
  payload += "\"payload_not_available\":\"offline\",";

  if (deviceClass && deviceClass[0]) {
    payload += "\"device_class\":\"" + String(deviceClass) + "\",";
  }
  if (icon && icon[0]) {
    payload += "\"icon\":\"" + String(icon) + "\",";
  }
  if (diagnostic) {
    payload += "\"entity_category\":\"diagnostic\",";
  }

  payload += mqttDeviceBlock();
  payload += "}";

  client.publish(topic.c_str(), payload.c_str(), true);
}

void publishHADiscovery()
{
  Serial.println("[MQTT] Publishing Home Assistant discovery");

  // Remove retained discovery entries from the old per-topic MQTT layout.
  client.publish("homeassistant/sensor/pvrouter-production/config", "", true);
  client.publish("homeassistant/sensor/pvrouter-consumption/config", "", true);
  client.publish("homeassistant/sensor/pvrouter-status/config", "", true);
  client.publish("homeassistant/binary_sensor/pvrouter-fronius/config", "", true);

  publishSensorDiscovery("pvrouter_pv", "Production PV", "pv_w",
                         "W", "power", "measurement", "mdi:solar-power");
  publishSensorDiscovery("pvrouter_grid", "Réseau", "grid_w",
                         "W", "power", "measurement", "mdi:transmission-tower");
  publishSensorDiscovery("pvrouter_house", "Consommation maison", "house_w",
                         "W", "power", "measurement", "mdi:home-lightning-bolt");
  publishSensorDiscovery("pvrouter_available", "Puissance disponible", "available_w",
                         "W", "power", "measurement", "mdi:flash");
  publishSensorDiscovery("pvrouter_heater", "Puissance chauffe-eau", "heater_w",
                         "W", "power", "measurement", "mdi:water-boiler");
  publishSensorDiscovery("pvrouter_dimmer_cmd", "Consigne dimmer", "dimmer_cmd",
                         "%", "", "measurement", "mdi:tune");
  publishSensorDiscovery("pvrouter_dimmer_actual", "Dimmer réel", "dimmer_actual",
                         "%", "", "measurement", "mdi:water-boiler-auto");
  publishSensorDiscovery("pvrouter_water_temp", "Température chauffe-eau", "water_temp",
                         "°C", "temperature", "measurement", "mdi:thermometer");
  publishSensorDiscovery("pvrouter_water_temp_max", "Température maxi chauffe-eau", "water_temp_max",
                         "°C", "temperature", "", "mdi:thermometer-check");
  publishSensorDiscovery("pvrouter_wifi", "WiFi RSSI", "wifi_rssi",
                         "dBm", "signal_strength", "measurement", "mdi:wifi", true);
  publishSensorDiscovery("pvrouter_state", "État", "status",
                         "", "", "", "mdi:information-outline");

  publishBinarySensorDiscovery("pvrouter_fronius", "Fronius", "fronius",
                               "connectivity", "mdi:solar-power", true);
  publishBinarySensorDiscovery("pvrouter_dimmer_online", "Dimmer", "dimmer_online",
                               "connectivity", "mdi:water-boiler", true);
  publishBinarySensorDiscovery("pvrouter_dimmer_synced", "Dimmer synchronisé", "dimmer_synced",
                               "", "mdi:sync", true);
}

void mqttCallback(char* topic, byte* payload, unsigned int length)
{
  // No command topic is currently required.
}

void Mqtt_publishState()
{
  if (!client.connected()) {
    return;
  }

  const unsigned long now = millis();
  const bool dimmerOnline =
      gDisplayValues.dimmerCommOk &&
      gDisplayValues.dimmerLastOkMs > 0 &&
      ((unsigned long)(now - gDisplayValues.dimmerLastOkMs) <= 15000UL);

  const int configuredHeaterPowerW =
      constrain(config.heaterPowerW > 0 ? config.heaterPowerW : 800, 100, 5000);
  const int configuredMaxDimmer =
      constrain(config.dimmerMaxPercent > 0 ? config.dimmerMaxPercent : 100, 10, 100);

  int dimmerCmd = gDisplayValues.dimmer;
  if (dimmerCmd < 0) dimmerCmd = 0;
  if (dimmerCmd > configuredMaxDimmer) dimmerCmd = configuredMaxDimmer;

  int dimmerActual = dimmerOnline ? gDisplayValues.dimmerReported : 0;
  if (dimmerActual < 0) dimmerActual = 0;
  if (dimmerActual > 100) dimmerActual = 100;

  const int heaterPower = (configuredHeaterPowerW * dimmerActual) / 100;
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
      dimmerOnline &&
      (abs(dimmerCmd - dimmerActual) <= 2);

  const bool tempAtOrAboveMax =
      (waterTemp > 0.0f) &&
      (effectiveMaxTemp > 0) &&
      (waterTemp >= (float)effectiveMaxTemp);

  String status;
  if (!gDisplayValues.froniusup) {
    status = "FRONIUS OFFLINE";
  }
  else if (!dimmerOnline) {
    status = "DIMMER OFFLINE";
  }
  else if (tempAtOrAboveMax && dimmerCmd > 0 && dimmerActual == 0) {
    status = "TEMP MAX";
  }
  else if (!dimmerSynced) {
    status = "CE SYNC";
  }
  else if (dimmerCmd >= configuredMaxDimmer && gridPower < -20) {
    status = "LOAD LIMITED";
  }
  else if (gridPower > 20) {
    status = "IMPORT";
  }
  else if (gridPower < -20) {
    status = "SURPLUS";
  }
  else {
    status = "ZERO GRID";
  }

  const int wifiRssi = WiFi.isConnected() ? WiFi.RSSI() : -127;

  String payload;
  payload.reserve(500);
  payload = "{";
  payload += "\"pv_w\":" + String(pvPower) + ",";
  payload += "\"grid_w\":" + String(gridPower) + ",";
  payload += "\"house_w\":" + String(housePower) + ",";
  payload += "\"available_w\":" + String(availablePower) + ",";
  payload += "\"heater_w\":" + String(heaterPower) + ",";
  payload += "\"heater_model_w\":" + String(configuredHeaterPowerW) + ",";
  payload += "\"dimmer_max\":" + String(configuredMaxDimmer) + ",";
  payload += "\"dimmer_cmd\":" + String(dimmerCmd) + ",";
  payload += "\"dimmer_actual\":" + String(dimmerActual) + ",";
  payload += "\"water_temp\":";
  if (waterTemp > 0.0f) payload += String(waterTemp, 1);
  else payload += "null";
  payload += ",";
  payload += "\"water_temp_max\":";
  if (remoteMaxTemp > 0) payload += String(remoteMaxTemp);
  else payload += "null";
  payload += ",";
  payload += "\"wifi_rssi\":" + String(wifiRssi) + ",";
  payload += "\"fronius\":" + String(gDisplayValues.froniusup ? "true" : "false") + ",";
  payload += "\"dimmer_online\":" + String(dimmerOnline ? "true" : "false") + ",";
  payload += "\"dimmer_synced\":" + String(dimmerSynced ? "true" : "false") + ",";
  payload += "\"status\":\"" + status + "\"";
  payload += "}";

  client.publish(PVROUTER_STATE_TOPIC, payload.c_str(), true);
  client.loop();
}

void reconnect()
{
  if (!client.connected()) {
    Serial.print("[MQTT] Connecting...");

    if (client.connect("pvrouter", MQTT_USER, MQTT_PASSWORD,
                       PVROUTER_AVAILABILITY_TOPIC,
                       0, true, "offline")) {
      Serial.println("connected");
      client.publish(PVROUTER_AVAILABILITY_TOPIC, "online", true);
      publishHADiscovery();
      Mqtt_publishState();
    }
    else {
      Serial.printf("failed rc=%d\n", client.state());
    }
  }
}

// Legacy helper kept for compatibility with older code paths. New V13 state is
// published as a single retained JSON document on pvrouter/state.
void Mqtt_send(String sensor, String value)
{
  if (!client.connected()) {
    return;
  }

  String state = "homeassistant/sensor/" + sensor + "/state";
  client.publish(state.c_str(), value.c_str(), true);
  client.loop();
}

void Mqtt_init()
{
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(mqttCallback);
  client.setKeepAlive(60);
  client.setBufferSize(1024);
}

#endif
