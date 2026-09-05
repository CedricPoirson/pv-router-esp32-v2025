#include "AsyncElegantOTA.h"
#include <Update.h>

AsyncElegantOtaClass AsyncElegantOTA;

void AsyncElegantOtaClass::begin(AsyncWebServer *server, const char* username, const char* password) {
  server->on("/update", HTTP_GET, [=](AsyncWebServerRequest *request){
    if (!request->authenticate(username, password)) return request->requestAuthentication();
    request->send(200, "text/html", "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>");
  });

  server->on("/update", HTTP_POST, [=](AsyncWebServerRequest *request){
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    response->addHeader("Connection", "close");
    request->send(response);
    ESP.restart();
  }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    if (!index){
      Update.begin(UPDATE_SIZE_UNKNOWN);
    }
    Update.write(data, len);
    if (final) {
      Update.end(true);
    }
  });
}

void AsyncElegantOtaClass::loop() {
  // Déprécié, mais laissé vide pour compatibilité
}
