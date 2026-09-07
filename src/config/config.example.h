#ifndef CONFIG
#define CONFIG

#include "version.h"

/**
 * Core features
 */
#define WEBSSERVER true
#define WIFI_ACTIVE true
#define MQTT_CLIENT true
#define HA_ENABLED false
#define AWS_ENABLED false

/**
 * WiFi credentials
 *
 * If WIFI_PASSWORD is left as "xxx", the firmware falls back to /wifi.json
 * stored in SPIFFS.
 */
#define WIFI_NETWORK "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

/**
 * Fronius inverter / Smart Meter
 *
 * This branch uses the Fronius Solar API v1 PowerFlow endpoint over HTTP.
 */
#define IP_FRONIUS "192.168.100.245"

/**
 * MQTT / Home Assistant
 */
#define MQTT_SERVER "192.168.100.20"
#define MQTT_PORT 1883
#define MQTT_USER "YOUR_MQTT_USER"
#define MQTT_PASSWORD "YOUR_MQTT_PASSWORD"

#define DEVICE_NAME "PVRouter ESP32"
#define DEVICE_ID "pvrouter-esp32"
#define DEVICE_MODEL "TTGO T-Display"
#define DEVICE_MANUF "Cédric Poirson"
#define DEVICE_VERSION PV_ROUTER_FIRMWARE_VERSION
#define HA_DISCOVERY_PREFIX "homeassistant"

/**
 * Display
 */
#define SWITCH 35
//#define SWITCHTIMER 0   // 0 : always ON / other : time in sec

/**
 * Legacy temperature refresh setting.
 * The current RobotDyn /state task uses its own adaptive 2 s / 5 s polling.
 */
#define GETTEMPREFRESH 30

/**
 * Set this to false to disable Serial logging.
 */
#define DEBUG true

/**
 * ADC inputs
 */
#ifdef DEVKIT1
#define ADC_INPUT 32
#define ADC_PORTEUSE 33
#endif

#ifdef TTGO
#define ADC_INPUT 32
#define ADC_PORTEUSE 33
#endif

#define ADC_MIDDLE 1893

/**
 * Home voltage used by the legacy local measurement path.
 */
#define HOME_VOLTAGE 225.0

/**
 * Dimmer
 */
#define DIMMER true
#define DIMMERLOCAL false
#define DALLAS false

#if DIMMERLOCAL
#define outputPin 26
#define zerocross 27
#endif

#if DALLAS
#define dallaspin 37
#endif

/**
 * WiFi recovery
 */
#define WIFI_TIMEOUT 20000
#define WIFI_RECOVER_TIME_MS 20000

/**
 * Display dimensions
 */
#define OLED_ON true
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

/**
 * Legacy CT measurement settings
 */
#define emonTxV3 1
#define LOCAL_MEASUREMENTS 30
#define FACTEURPUISSANCE 10.50

/**
 * NTP
 */
#define NTP_TIME_SYNC_ENABLED true
#define NTP_SERVER "europe.pool.ntp.org"
#define NTP_OFFSET_SECONDS 3600
#define NTP_UPDATE_INTERVAL_MS 3600000

/**
 * Optional AWS support is disabled by default.
 * Add the required endpoint/certificate configuration before enabling it.
 */
//#define AWS_IOT_ENDPOINT "YOUR_AWS_IOT_ENDPOINT"
//#define AWS_IOT_TOPIC "YOUR_AWS_IOT_TOPIC"

/**
 * Display updates must run on the Arduino core.
 * Some ESP32 framework versions already define ARDUINO_RUNNING_CORE.
 */
#ifndef ARDUINO_RUNNING_CORE
  #if CONFIG_FREERTOS_UNICORE
    #define ARDUINO_RUNNING_CORE 0
  #else
    #define ARDUINO_RUNNING_CORE 1
  #endif
#endif

// Kept for legacy helpers that still print VERSION.
#define VERSION PV_ROUTER_FIRMWARE_LABEL

#endif
