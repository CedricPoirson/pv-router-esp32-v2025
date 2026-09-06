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
  String sensors[] = {"pvrouter-production", "pvrouter-consumption", "pvrouter-surplus"};

  for (String sensor : sensors) {
    String config = "homeassistant/sensor/" + sensor + "/config";
    String state = "homeassistant/sensor/" + sensor + "/state";
    String status = "homeassistant/sensor/" + sensor + "/status";

    String payload = "{";
    payload += "\"name\":\"PVRouter " + sensor + "\",";
    payload += "\"unique_id\":\"" + sensor + "_esp32\",";
    payload += "\"state_topic\":\"" + state + "\",";
    payload += "\"availability_topic\":\"" + status + "\",";
    payload += "\"payload_available\":\"online\",";
    payload += "\"payload_not_available\":\"offline\",";
    payload += "\"device_class\":\"power\",";
    payload += "\"unit_of_measurement\":\"W\"}";

    client.publish(config.c_str(), payload.c_str(), true);
  }

  String config = "homeassistant/binary_sensor/pvrouter-fronius/config";
  String payload = "{\"name\":\"PVRouter Fronius\",\"unique_id\":\"pvrouter_fronius\",\"state_topic\":\"homeassistant/binary_sensor/pvrouter-fronius/state\",\"payload_on\":\"ON\",\"payload_off\":\"OFF\",\"device_class\":\"connectivity\"}";
  client.publish(config.c_str(), payload.c_str(), true);
}

void reconnect() {
  if (!client.connected()) {
    Serial.print("Attempting MQTT connection...");

    if (client.connect("pvrouter", MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("connected");

      publishHADiscovery();

      Mqtt_send("pvrouter-production", String(int(gDisplayValues.production)));
      Mqtt_send("pvrouter-consumption", String(int(gDisplayValues.watt)));
      Mqtt_send("pvrouter-surplus", String(int(gDisplayValues.surplus)));

      client.publish("homeassistant/binary_sensor/pvrouter-fronius/state", gDisplayValues.froniusup ? "ON" : "OFF", true);
    }
  }
}

void Mqtt_loop() {
  if (client.connected()) client.loop();
  else reconnect();
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

  Serial.print("Connexion MQTT à ");
  Serial.println(MQTT_SERVER);
  Serial.println("MQTT initialisé");
}

#endif
