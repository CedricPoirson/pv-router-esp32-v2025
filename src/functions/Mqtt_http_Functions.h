#ifndef MQTT_FUNCTIONS
#define MQTT_FUNCTIONS

#include <Arduino.h>
#include "../config/config.h"
#include "../config/enums.h"
#include "functions/spiffsFunctions.h"
#include <PubSubClient.h>
#include <WiFi.h>
#include "HTTPClient.h"

WiFiClient espClient;
PubSubClient client(espClient);

extern DisplayValues gDisplayValues;

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("pvrouter", MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void Mqtt_loop() {
  if (client.connected()) {
    client.loop();
  } else {
    reconnect();
  }
}

void Mqtt_send(String sensor, String value) {
  String topic_state = "homeassistant/sensor/" + sensor + "/state";
  String topic_config = "homeassistant/sensor/" + sensor + "/config";
  String topic_status = "homeassistant/sensor/" + sensor + "/status";

  static String sentSensors[10];
  static int sentCount = 0;

  bool config_sent = false;
  for (int i = 0; i < sentCount; i++) {
    if (sentSensors[i] == sensor) {
      config_sent = true;
      break;
    }
  }

  if (!config_sent && sentCount < 10) {
    String payload_config = "{";
    payload_config += "\"name\": \"Puissance " + sensor + "\",";
    payload_config += "\"state_topic\": \"" + topic_state + "\",";
    payload_config += "\"unit_of_measurement\": \"W\",";
    payload_config += "\"device_class\": \"power\",";
    payload_config += "\"state_class\": \"measurement\",";
    payload_config += "\"availability_topic\": \"" + topic_status + "\",";
    payload_config += "\"force_update\": true,";
    payload_config += "\"unique_id\": \"pvrouter-" + sensor + "\",";
    payload_config += "\"device\": {\"name\": \"PVRouter ESP32\",\"identifiers\": [\"pvrouter-esp32\"],\"manufacturer\": \"Cédric Poirson\",\"model\": \"TTGO T-Display\",\"sw_version\": \"1.0\"}}";

    client.publish(topic_config.c_str(), payload_config.c_str(), true);
    sentSensors[sentCount++] = sensor;
  }

  client.publish(topic_status.c_str(), "online", true);
  client.publish(topic_state.c_str(), value.c_str(), true);
}

void Mqtt_init() {
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setKeepAlive(60);

  Serial.print("Connexion MQTT à ");
  Serial.println(MQTT_SERVER);
  Serial.println("MQTT initialisé");

}

#endif
