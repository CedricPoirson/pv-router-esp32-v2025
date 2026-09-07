#include <Arduino.h>

#include "WiFi.h"
#include <driver/adc.h>
#include "config/config.h"
#include "config/enums.h"
#include "config/traduction.h"
#include <NTPClient.h>
#include <AsyncElegantOTA.h>

// File System
#include <FS.h>
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include <ArduinoJson.h> // ArduinoJson : https://github.com/bblanchon/ArduinoJson

#include "tasks/updateDisplay.h"
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
#include "functions/spiffsFunctions.h"
#include "functions/Mqtt_http_Functions.h"
#include "functions/webFunctions.h"

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
      drawTTGOBootScreen("DEMARRAGE", "Initialisation materiel", 15);
    #endif
  #endif

  #if WIFI_ACTIVE == true
    #ifdef TTGO
      drawTTGOBootScreen("CONNEXION WI-FI", "Connexion au reseau local", 35);
    #endif

    if (strcmp(WIFI_PASSWORD, "xxx") == 0) {
      WiFi.begin(configwifi.SID, configwifi.passwd);
    }
    else {
      WiFi.begin(WIFI_NETWORK, WIFI_PASSWORD);
    }

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    serial_println("WiFi connected");
    serial_println("IP address: ");
    serial_println(WiFi.localIP());
    gDisplayValues.currentState = UP;
    gDisplayValues.IP = String(WiFi.localIP().toString());
    btStop();

    #ifdef TTGO
      drawTTGOBootScreen("WI-FI OK", gDisplayValues.IP, 60);
    #endif
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
    drawTTGOBootScreen("CONFIGURATION", "Lecture config.json", 75);
  #endif

  Serial.println(F("Loading configuration..."));
  loadConfiguration(filename_conf, config);

  #ifdef TTGO
    drawTTGOBootScreen("CONFIGURATION OK", "Demarrage des services", 82);
  #endif

  // Initialize Dimmer State
  gDisplayValues.dimmer = 0;

  #if WIFI_ACTIVE == true
    #if WEBSSERVER == true
      //***********************************
      //************* Setup - démarrage du webserver et affichage de l'oled
      //***********************************
      Serial.println("start Web server");
      call_pages();
      #ifdef TTGO
        drawTTGOBootScreen("SERVEUR WEB OK", gDisplayValues.IP, 90);
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
      updateDisplay,
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
      AsyncElegantOTA.begin(&server);
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
    drawTTGOBootScreen("PRET", "Fronius - RobotDyn - MQTT", 100);
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
