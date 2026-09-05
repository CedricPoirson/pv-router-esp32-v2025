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

    String payload = "{";
    payload += "\"name\":\"" + sensor + "\",";
    payload += "\"unique_id\":\"" + sensor + "\",";
    payload += "\"state_topic\":\"" + state + "\",";
    payload += "\"availability_topic\":\"" + status + "\",";
    payload += "\"payload_available\":\"online\",";
    payload += "\"payload_not_available\":\"offline\"";

    if (sensor != "pvrouter-status") {
      payload += ",\"unit_of_measurement\":\"W\"";
      payload += ",\"device_class\":\"power\"";
    }

    payload += "}";

    bool ok = client.publish(config.c_str(), payload.c_str(), true);
    Serial.print("MQTT Discovery ");
    Serial.print(config);
    Serial.println(ok ? " OK" : " FAILED");
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
}

void reconnect() {
  if (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("pvrouter", MQTT_USER, MQTT_PASSWORD,
                       "homeassistant/sensor/pvrouter-status/status",
                       0, true, "offline")) {
      Serial.println("connected");
      publishHADiscovery();
      Mqtt_send("pvrouter-status", String(int(gDisplayValues.froniusup)));
      Mqtt_send("pvrouter-production", String(int(gDisplayValues.production)));
      Mqtt_send("pvrouter-consumption", String(int(gDisplayValues.watt)));
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
  client.setCallback(mqttCallback);
  client.setKeepAlive(60);
}

#endif
