#ifndef TASK_SWITCH_DISPLAY
#define TASK_SWITCH_DISPLAY

#include <Arduino.h>

#include "../config/config.h"

#define TFT_PIN 4
extern DisplayValues gDisplayValues;
extern Config config;

#ifdef TTGO
extern volatile uint8_t gDisplayPage;
extern volatile bool gDisplayForceRefresh;
#endif

void switchDisplay(void * parameter){
  unsigned long screenOnSince = millis();
  unsigned long buttonDownSince = 0;
  bool buttonDown = false;

  const unsigned long LONG_PRESS_MS = 800UL;
  const unsigned long DEBOUNCE_MS = 40UL;

  for(;;){
    const unsigned long now = millis();
    const bool buttonPressed = (digitalRead(SWITCH) == LOW);

    // Detect the beginning of a physical button press.
    if (buttonPressed && !buttonDown) {
      buttonDown = true;
      buttonDownSince = now;
    }

    // Act on button release so short and long presses are unambiguous.
    if (!buttonPressed && buttonDown) {
      const unsigned long pressDuration = now - buttonDownSince;
      buttonDown = false;

      if (pressDuration >= DEBOUNCE_MS) {
        const bool displayOn = (digitalRead(TFT_PIN) == HIGH);

        if (!displayOn) {
          // Any press wakes the display and returns to the main family page.
          digitalWrite(TFT_PIN, HIGH);
#ifdef TTGO
          gDisplayPage = 0;
          gDisplayForceRefresh = true;
#endif
          screenOnSince = now;
        }
        else if (pressDuration >= LONG_PRESS_MS) {
          // Long press: keep the historical ability to switch the screen off.
          digitalWrite(TFT_PIN, LOW);
        }
        else {
#ifdef TTGO
          // Short press: toggle family dashboard <-> diagnostic page.
          gDisplayPage = (gDisplayPage == 0) ? 1 : 0;
          gDisplayForceRefresh = true;
#endif
          screenOnSince = now;
        }
      }
    }

    // Preserve the existing HTTP-triggered screen toggle behaviour.
    if (gDisplayValues.screenstate == HIGH) {
      gDisplayValues.screenstate = LOW;

      if (digitalRead(TFT_PIN) == HIGH) {
        digitalWrite(TFT_PIN, LOW);
      }
      else {
        digitalWrite(TFT_PIN, HIGH);
#ifdef TTGO
        gDisplayPage = 0;
        gDisplayForceRefresh = true;
#endif
        screenOnSince = now;
      }
    }

    // Optional automatic screen timeout from the existing configuration.
    if (digitalRead(TFT_PIN) == HIGH && config.ScreenTime != 0) {
      if ((unsigned long)(now - screenOnSince) >=
          (unsigned long)config.ScreenTime * 1000UL) {
        digitalWrite(TFT_PIN, LOW);
      }
    }

    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

#endif
