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

/***
 *  Reconnexion au serveur MQTT
 */
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("arduinoClient", MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

/***
 *  Envoi des données MQTT en format compatible Home Assistant Discovery
 */
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
    payload_config += "\"device\": {";
    payload_config += "\"name\": \"PVRouter ESP32\",";
    payload_config += "\"identifiers\": [\"pvrouter-esp32\"],";
    payload_config += "\"manufacturer\": \"Cédric Poirson\",";
    payload_config += "\"model\": \"TTGO T-Display\",";
    payload_config += "\"sw_version\": \"1.0\"";
    payload_config += "}}";

    Serial.print("MQTT DISCOVERY : ");
    Serial.println(topic_config);
    client.publish(topic_config.c_str(), payload_config.c_str(), true);
    sentSensors[sentCount++] = sensor;
  }

  Serial.print("MQTT STATUS : ");
  Serial.println(topic_status);
  Serial.print("MQTT STATE : ");
  Serial.print(topic_state);
  Serial.print(" = ");
  Serial.println(value);

  client.publish(topic_status.c_str(), "online", true);
  client.publish(topic_state.c_str(), value.c_str(), true);
}

/***
 *  Initialisation du client MQTT
 */
void Mqtt_init() {
  client.setServer(MQTT_SERVER, MQTT_PORT);

  Serial.print("Connexion MQTT à ");
  Serial.println(MQTT_SERVER);

  bool ok = client.connect("pvrouter", MQTT_USER, MQTT_PASSWORD);

  if (ok) {
    Serial.println("MQTT connecté !");
    Mqtt_send(String(config.IDXdimmer), "0");
  } else {
    Serial.print("MQTT échec, rc=");
    Serial.println(client.state());
  }
}

#endif
