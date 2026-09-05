#pragma once

#include <Arduino.h>
#ifdef ESP8266
  #include <ESP8266WiFi.h>
  #include <ESPAsyncTCP.h>
#elif defined(ESP32)
  #include <WiFi.h>
  #include <AsyncTCP.h>
#endif

#include <ESPAsyncWebServer.h>

class AsyncElegantOtaClass {
  public:
    void begin(AsyncWebServer *server, const char* username = "", const char* password = "");
    void loop();
};

extern AsyncElegantOtaClass AsyncElegantOTA;
