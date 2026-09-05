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

  // On envoie la config seulement au démarrage
  static bool config_sent = false;
  if (!config_sent) {
    String payload_config = "{";
    payload_config += "\"name\": \"Puissance Chauffe-eau " + sensor + "\",";
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

    client.publish(topic_config.c_str(), payload_config.c_str(), true);
    config_sent = true;
  }

  // Envoi de présence + valeur
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
