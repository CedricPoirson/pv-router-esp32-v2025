#ifndef CONFIG
#define CONFIG

// -----------------------------------------------------------------------------
// Réseau
// -----------------------------------------------------------------------------
#define WEBSSERVER true
#define WIFI_ACTIVE true
#define MQTT_CLIENT true

#define WIFI_NETWORK "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

#define MQTT_PORT 1883
#define MQTT_USER "YOUR_MQTT_USER"
#define MQTT_PASSWORD "YOUR_MQTT_PASSWORD"

// -----------------------------------------------------------------------------
// Matériel utilisé
// -----------------------------------------------------------------------------
#define DEVICE_NAME "PVRouter ESP32"
#define DEVICE_ID "pvrouter-esp32"
#define DEVICE_MODEL "LilyGO T-Display"
#define DEVICE_MANUF "Cédric Poirson"
#define DEVICE_VERSION "1.0"

#define SWITCH 35
#define IP_FRONIUS "192.168.100.245"
#define GETTEMPREFRESH 30
#define DEBUG true

#ifdef DEVKIT1
#define ADC_INPUT 32
#define ADC_PORTEUSE 33
#endif

#ifdef TTGO
#define ADC_INPUT 32
#define ADC_PORTEUSE 33
#endif

#define ADC_MIDDLE 1893
#define HOME_VOLTAGE 225.0

// -----------------------------------------------------------------------------
// Dimmer
// -----------------------------------------------------------------------------
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

// -----------------------------------------------------------------------------
// Wi-Fi
// -----------------------------------------------------------------------------
#define WIFI_TIMEOUT 20000
#define WIFI_RECOVER_TIME_MS 20000

// -----------------------------------------------------------------------------
// Affichage
// -----------------------------------------------------------------------------
#define OLED_ON true
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

// -----------------------------------------------------------------------------
// Ancienne mesure locale - conservée temporairement pour compatibilité
// -----------------------------------------------------------------------------
#define emonTxV3 1
#define LOCAL_MEASUREMENTS 30
#define FACTEURPUISSANCE 10.50

// -----------------------------------------------------------------------------
// Heure
// NTP_OFFSET_SECONDS sert uniquement d'offset initial à NTPClient.
// La tâche NTP applique ensuite automatiquement UTC+1 / UTC+2 pour Paris.
// -----------------------------------------------------------------------------
#define NTP_TIME_SYNC_ENABLED true
#define NTP_SERVER "europe.pool.ntp.org"
#define NTP_OFFSET_SECONDS 3600
#define NTP_UPDATE_INTERVAL_MS 3600000

// -----------------------------------------------------------------------------
// Options historiques non utilisées dans la configuration actuelle
// -----------------------------------------------------------------------------
//#define HA_ENABLED false
//#define AWS_ENABLED false

#define VERSION "version 3.6"
#define HA_DISCOVERY_PREFIX "homeassistant"

#endif
