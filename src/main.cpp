#include <Arduino.h>

#include "WiFi.h"
#include <driver/adc.h>
#include "config/config.h"
#include "config/enums.h"
#include "config/traduction.h"
#include <NTPClient.h>

// File System
#include <FS.h>
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include <ArduinoJson.h> // ArduinoJson : https://github.com/bblanchon/ArduinoJson

// SPIFFS helpers are intentionally included before the Wi-Fi tasks so the
// reconnect path and the physical-button setup portal share the same stored
// credential helpers.
#include "functions/spiffsFunctions.h"
#include "functions/wifiSetupPortal.h"

#include "tasks/updateDisplay.h"
#include "tasks/smoothDisplay.h"
#include "tasks/versionedDisplay.h"
#include "tasks/bootScreen.h"
#include "tasks/switchDisplay.h"
#include "tasks/fetch-time-from-ntp.h"
//#include "tasks/mqtt-aws.h"
#include "tasks/wifi-connection.h"
//#include "tasks/wifi-update-signalstrength.h"
#include "tasks/measure-electricity.h"
//#include "tasks/mqtt-home-assistant.h"
#include "tasks/Dimmer.h"
#include "tasks/gettemp.h"

#include "functions/otaFunctions.h"
#include "functions/Mqtt_http_Functions.h"
#include "functions/webFunctions.h"
#include "functions/webOta.h"

#if DIMMERLOCAL
#include "functions/dimmerFunction.h"
#endif

//***********************************
//************* Afficheur Oled
//***********************************
#ifdef DEVKIT1
// Oled
#include "SSD1306Wire.h" /// Oled ( https://github.com/ThingPulse/esp8266-oled-ssd1306 )
const int I2C_DISPLAY_ADDRESS = 0x3c;
SSD1306Wire display(0x3c, SDA, SCL); // pin 21 SDA - 22 SCL
#endif

#ifdef TTGO
#include <TFT_eSPI.h>
#include <SPI.h>
TFT_eSPI display = TFT_eSPI();   // Invoke library
#endif

DisplayValues gDisplayValues;
Config config;
Configwifi configwifi;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_SERVER, NTP_OFFSET_SECONDS, NTP_UPDATE_INTERVAL_MS);
// NTPClient applies this offset inside getEpochTime(). Keep the active
// Europe/Paris value available so absolute UTC timestamps (e.g. Solcast
// valid_until sent by Home Assistant) can be compared correctly.
long gNtpParisOffsetSeconds = NTP_OFFSET_SECONDS;

// Place to store local measurements before sending them off to AWS
unsigned short measurements[LOCAL_MEASUREMENTS];
unsigned char measureIndex = 0;

void setup()
{
  #if DEBUG == true
    Serial.begin(115200);
  #endif

  // démarrage file system
  Serial.println("start SPIFFS");
  SPIFFS.begin();
  loadwifi(wifi_conf, configwifi);

  // Setup the ADC
  adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);
  pinMode(ADC_INPUT, INPUT);

  #if OLED_ON == true
    Serial.println(OLEDSTART);
    // Initialising OLED
    #ifdef DEVKIT1
      display.init();
      display.flipScreenVertically();
      display.clear();
    #endif

    #ifdef TTGO
      pinMode(SWITCH, INPUT);
      display.init();
      display.setRotation(1);
      drawTTGOGraphicalBootScreen("DEMARRAGE", "Initialisation materiel", 15);

      // Physical setup entry point: the access point is never exposed merely
      // because the home Wi-Fi is unavailable. It requires a deliberate
      // three-second button hold during power-up/reboot.
      if (wifiSetupRequestedAtBoot()) {
        runWifiSetupPortal();
      }

      drawTTGOGraphicalBootScreen("DEMARRAGE", "Configuration Wi-Fi normale", 25);
    #endif
  #endif

  #if WIFI_ACTIVE == true
    #ifdef TTGO
      drawTTGOGraphicalBootScreen("CONNEXION WI-FI", "Connexion au reseau local", 35);
    #endif

    beginConfiguredWiFi();
    const unsigned long firstWifiAttempt = millis();

    while (WiFi.status() != WL_CONNECTED &&
           millis() - firstWifiAttempt < WIFI_TIMEOUT) {
      delay(250);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      serial_println("WiFi connected");
      serial_println("IP address: ");
      serial_println(WiFi.localIP());
      gDisplayValues.currentState = UP;
      gDisplayValues.IP = String(WiFi.localIP().toString());
      btStop();

      #ifdef TTGO
        drawTTGOGraphicalBootScreen("WI-FI OK", gDisplayValues.IP, 60);
      #endif
    }
    else {
      serial_println("[WIFI] Initial connection failed; background retries enabled");
      gDisplayValues.currentState = CONNECTING_WIFI;
      gDisplayValues.IP = "OFFLINE";

      #ifdef TTGO
        drawTTGOGraphicalBootScreen("WI-FI INDISPONIBLE", "Boot + bouton 3 s = config", 60);
        delay(1300);
      #endif
    }
  #endif

  #if DIMMERLOCAL
    Dimmer_setup();
  #endif

  // vérification de la présence d'index.html
  if (!SPIFFS.exists("/index.html")) {
    Serial.println(SPIFFSNO);
  }

  if (!SPIFFS.exists(filename_conf)) {
    Serial.println(CONFNO);
  }

  //***********************************
  //************* Setup - récupération du fichier de configuration
  //***********************************
  #ifdef TTGO
    drawTTGOGraphicalBootScreen("CONFIGURATION", "Lecture config.json", 75);
  #endif

  Serial.println(F("Loading configuration..."));
  loadConfiguration(filename_conf, config);

  #ifdef TTGO
    drawTTGOGraphicalBootScreen("CONFIGURATION OK", "Demarrage des services", 82);
  #endif

  // Initialize Dimmer State
  gDisplayValues.dimmer = 0;

  #if WIFI_ACTIVE == true
    #if WEBSSERVER == true
      //***********************************
      //************* Setup - démarrage du webserver et affichage de l'oled
      //***********************************
      Serial.println("start Web server");
      // Register the OTA/branding routes first. This intentionally takes
      // precedence over the historical SPIFFS /favicon.ico handler.
      setupWebOta();
      call_pages();
      #ifdef TTGO
        drawTTGOGraphicalBootScreen("SERVEUR WEB OK", gDisplayValues.IP, 90);
      #endif
    #endif

    // TASK: Connect to WiFi & keep the connection alive.
    xTaskCreate(
      keepWiFiAlive,
      "keepWiFiAlive",
      5000,
      NULL,
      5,
      NULL
    );
  #endif

  // TASK: Connect to AWS & keep the connection alive.
  #if AWS_ENABLED == true
    xTaskCreate(
      keepAWSConnectionAlive,
      "MQTT-AWS",
      5000,
      NULL,
      5,
      NULL
    );
  #endif

  // TASK: Update the display every second.
  #if OLED_ON == true
    xTaskCreatePinnedToCore(
      updateDisplaySmoothV144,
      "UpdateDisplay",
      10000,
      NULL,
      4,
      NULL,
      ARDUINO_RUNNING_CORE
    );
  #endif

  #ifdef TTGO
    xTaskCreate(
      switchDisplay,
      "Switch Oled",
      1000,
      NULL,
      2,
      NULL
    );
  #endif

  // TASK: measure Fronius / grid power.
  xTaskCreate(
    measureElectricityf,
    "Measure electricity",
    10000,
    NULL,
    25,
    NULL
  );

  #if WIFI_ACTIVE == true
    #if DIMMER == true
      // TASK: regulate dimmer from fresh Fronius samples.
      xTaskCreate(
        updateDimmer,
        "Update Dimmer",
        5000,
        NULL,
        4,
        NULL
      );

      // TASK: poll RobotDyn /state telemetry.
      xTaskCreate(
        GetDImmerTemp,
        "Update temp",
        5000,
        NULL,
        4,
        NULL
      );
    #endif
  #endif

  // TASK: update time from NTP server.
  #if WIFI_ACTIVE == true
    #if NTP_TIME_SYNC_ENABLED == true
      xTaskCreate(
        fetchTimeFromNTP,
        "Update NTP time",
        5000,
        NULL,
        2,
        NULL
      );
    #endif

    #if HA_ENABLED == true
      xTaskCreate(
        HADiscovery,
        "MQTT-HA Discovery",
        5000,
        NULL,
        5,
        NULL
      );

      xTaskCreate(
        keepHAConnectionAlive,
        "MQTT-HA Connect",
        5000,
        NULL,
        4,
        NULL
      );
    #endif
  #endif

  #if WIFI_ACTIVE == true
    #if WEBSSERVER == true
      server.begin();
    #endif

    #if MQTT_CLIENT == true
      Mqtt_init();
    #endif

    if (config.autonome == true) {
      gDisplayValues.dimmer = 0;
      dimmer_change(config.dimmer, config.IDXdimmer, gDisplayValues.dimmer);
    }
  #endif

  #ifdef TTGO
    if (WiFi.status() == WL_CONNECTED)
      drawTTGOGraphicalBootScreen("PRET", "Fronius - RobotDyn - MQTT", 100);
    else
      drawTTGOGraphicalBootScreen("WI-FI EN ATTENTE", "Bouton 3 s au boot = config", 100);

    delay(550);
    gDisplayBootComplete = true;
    gDisplayForceRefresh = true;
  #endif

  #if OLED_ON == true
    #ifdef DEVKIT1
      display.clear();
    #endif
  #endif
}

void loop()
{
  #if WIFI_ACTIVE == true
    #if MQTT_CLIENT == true
      if (!client.connected()) {
        reconnect();
      }
    #endif
  #endif

  vTaskDelay(10000 / portTICK_PERIOD_MS);
}
