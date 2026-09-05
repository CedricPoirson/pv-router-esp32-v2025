#ifndef MQTT_FUNCTIONS
#define MQTT_FUNCTIONS

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include "../config/config.h"
#include "../config/enums.h"

WiFiClient espClient;
PubSubClient client(espClient);

extern DisplayValues gDisplayValues;
extern Config config;

SemaphoreHandle_t mqttMutex = nullptr;
TaskHandle_t mqttTaskHandle = nullptr;
bool mqttGridDiscoverySent = false;
bool mqttDimmerDiscoverySent = false;

static bool mqttTakeLock(TickType_t timeout = pdMS_TO_TICKS(1000)) {
  return mqttMutex != nullptr && xSemaphoreTake(mqttMutex, timeout) == pdTRUE;
}

static void mqttReleaseLock() {
  if (mqttMutex != nullptr) {
    xSemaphoreGive(mqttMutex);
  }
}

/**
 * Tente une seule reconnexion MQTT.
 * La temporisation entre deux essais est gérée par la tâche MQTT.
 */
void reconnect() {
  if (!config.mqtt || !WiFi.isConnected() || client.connected()) {
    return;
  }

  if (!mqttTakeLock()) {
    return;
  }

  if (!client.connected()) {
    String clientId = "pvrouter-";
    clientId += String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFF), HEX);

    serial_print("[MQTT] Connecting to ");
    serial_println(config.mqttserver);

    if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
      serial_println("[MQTT] Connected");
      // Republier le discovery après une reconnexion est sans risque car il est retained.
      mqttGridDiscoverySent = false;
      mqttDimmerDiscoverySent = false;
    } else {
      serial_print("[MQTT] Connection failed, rc=");
      serial_println(client.state());
    }
  }

  mqttReleaseLock();
}

/**
 * Publication compatible Home Assistant Discovery.
 * Compatibilité conservée avec les anciens appels basés sur IDX / IDXdimmer.
 */
void Mqtt_send(String sensor, String value) {
  if (!config.mqtt || !client.connected() || mqttMutex == nullptr) {
    return;
  }

  const bool isDimmer = sensor == String(config.IDXdimmer);
  bool *discoverySent = isDimmer ? &mqttDimmerDiscoverySent : &mqttGridDiscoverySent;

  const String topic_state = "homeassistant/sensor/" + sensor + "/state";
  const String topic_config = "homeassistant/sensor/" + sensor + "/config";
  const String topic_status = "homeassistant/sensor/" + sensor + "/status";

  if (!mqttTakeLock()) {
    return;
  }

  if (!*discoverySent) {
    String payload_config = "{";
    payload_config += "\"name\":\"";
    payload_config += isDimmer ? "Commande dimmers" : "Puissance réseau";
    payload_config += "\",";
    payload_config += "\"state_topic\":\"" + topic_state + "\",";
    payload_config += "\"availability_topic\":\"" + topic_status + "\",";
    payload_config += "\"unique_id\":\"pvrouter-";
    payload_config += isDimmer ? "dimmer" : "grid";
    payload_config += "\",";

    if (isDimmer) {
      payload_config += "\"unit_of_measurement\":\"%\",";
    } else {
      payload_config += "\"unit_of_measurement\":\"W\",";
      payload_config += "\"device_class\":\"power\",";
      payload_config += "\"state_class\":\"measurement\",";
    }

    payload_config += "\"device\":{";
    payload_config += "\"name\":\"PVRouter ESP32\",";
    payload_config += "\"identifiers\":[\"pvrouter-esp32\"],";
    payload_config += "\"manufacturer\":\"Cédric Poirson\",";
    payload_config += "\"model\":\"LilyGO T-Display\",";
    payload_config += "\"sw_version\":\"" VERSION "\"";
    payload_config += "}}";

    if (client.publish(topic_config.c_str(), payload_config.c_str(), true)) {
      *discoverySent = true;
    }
  }

  client.publish(topic_status.c_str(), "online", true);
  client.publish(topic_state.c_str(), value.c_str(), true);

  mqttReleaseLock();
}

/**
 * Tâche dédiée à l'entretien de la connexion PubSubClient.
 */
void mqttLoopTask(void *parameter) {
  for (;;) {
    if (!config.mqtt) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    if (!WiFi.isConnected()) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    if (!client.connected()) {
      reconnect();
      vTaskDelay(pdMS_TO_TICKS(5000));
      continue;
    }

    if (mqttTakeLock(pdMS_TO_TICKS(100))) {
      client.loop();
      mqttReleaseLock();
    }

    vTaskDelay(pdMS_TO_TICKS(250));
  }
}

/**
 * Initialisation du client MQTT.
 */
void Mqtt_init() {
  if (!config.mqtt) {
    serial_println("[MQTT] Disabled in config.json");
    return;
  }

  if (mqttMutex == nullptr) {
    mqttMutex = xSemaphoreCreateMutex();
  }

  client.setServer(config.mqttserver, MQTT_PORT);
  client.setKeepAlive(30);
  client.setSocketTimeout(3);

  reconnect();

  if (mqttTaskHandle == nullptr) {
    xTaskCreate(
      mqttLoopTask,
      "MQTT loop",
      4096,
      nullptr,
      3,
      &mqttTaskHandle
    );
  }

  if (client.connected()) {
    Mqtt_send(String(config.IDXdimmer), String(gDisplayValues.dimmer));
    Mqtt_send(String(config.IDX), String(int(gDisplayValues.watt)));
  }
}

#endif
