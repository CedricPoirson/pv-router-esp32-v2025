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

void Mqtt_send(String sensor, String value);

void publishHADiscovery() {
  String sensors[] = {"pvrouter-production", "pvrouter-consumption", "pvrouter-status"};
  for (String sensor : sensors) {
    String state = "homeassistant/sensor/" + sensor + "/state";
    String config = "homeassistant/sensor/" + sensor + "/config";
    String status = "homeassistant/sensor/" + sensor + "/status";
    String payload = "{\"name\":\"" + sensor + "\",\"state_topic\":\"" + state + "\",\"availability_topic\":\"" + status + "\",\"unique_id\":\"" + sensor + "\"}";
    client.publish(config.c_str(), payload.c_str(), true);
  }
}

void reconnect() {
  if (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("pvrouter", MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("connected");
      publishHADiscovery();
    }
  }
}

void Mqtt_send(String sensor, String value) {
  String state = "homeassistant/sensor/" + sensor + "/state";
  String status = "homeassistant/sensor/" + sensor + "/status";
  client.publish(status.c_str(), "online", true);
  client.publish(state.c_str(), value.c_str(), true);
}

void Mqtt_init() {
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setKeepAlive(60);
  if (client.connect("pvrouter", MQTT_USER, MQTT_PASSWORD)) {
    Serial.println("MQTT connecté !");
    publishHADiscovery();
  }
}

#endif
