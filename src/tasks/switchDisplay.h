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
  unsigned long froniusDownSince = 0;
  unsigned long manualWakeUntil = 0;
  bool buttonDown = false;
  bool froniusAutoBlanked = false;

  const unsigned long LONG_PRESS_MS = 800UL;
  const unsigned long DEBOUNCE_MS = 40UL;
  const unsigned long FRONIUS_SCREEN_OFF_DELAY_MS = 10UL * 60UL * 1000UL;
  const unsigned long MANUAL_WAKE_GRACE_MS = 60UL * 1000UL;

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
          froniusAutoBlanked = false;

          // At night, keep a manual wake visible long enough to inspect it.
          if (!gDisplayValues.froniusup)
            manualWakeUntil = now + MANUAL_WAKE_GRACE_MS;
        }
        else if (pressDuration >= LONG_PRESS_MS) {
          // Long press: keep the historical ability to switch the screen off.
          digitalWrite(TFT_PIN, LOW);
          froniusAutoBlanked = false;
        }
        else {
#ifdef TTGO
          // Short press cycles through:
          //   0 = main dashboard
          //   1 = diagnostics
          //   2 = on-device legend / help
          gDisplayPage = (gDisplayPage + 1) % 3;
          gDisplayForceRefresh = true;
#endif
          screenOnSince = now;

          if (!gDisplayValues.froniusup)
            manualWakeUntil = now + MANUAL_WAKE_GRACE_MS;
        }
      }
    }

    // Preserve the existing HTTP-triggered screen toggle behaviour.
    if (gDisplayValues.screenstate == HIGH) {
      gDisplayValues.screenstate = LOW;

      if (digitalRead(TFT_PIN) == HIGH) {
        digitalWrite(TFT_PIN, LOW);
        froniusAutoBlanked = false;
      }
      else {
        digitalWrite(TFT_PIN, HIGH);
#ifdef TTGO
        gDisplayPage = 0;
        gDisplayForceRefresh = true;
#endif
        screenOnSince = now;
        froniusAutoBlanked = false;

        if (!gDisplayValues.froniusup)
          manualWakeUntil = now + MANUAL_WAKE_GRACE_MS;
      }
    }

    // Night behaviour: if the Fronius stays unreachable for 10 minutes,
    // switch off only the TFT backlight. The ESP32 keeps running normally.
    if (gDisplayValues.froniusup) {
      froniusDownSince = 0;
      manualWakeUntil = 0;

      // Wake automatically at sunrise / inverter return only if this logic
      // was responsible for blanking the display.
      if (froniusAutoBlanked) {
        digitalWrite(TFT_PIN, HIGH);
#ifdef TTGO
        gDisplayPage = 0;
        gDisplayForceRefresh = true;
#endif
        screenOnSince = now;
        froniusAutoBlanked = false;
        Serial.println("[DISPLAY] ON - Fronius online");
      }
    }
    else {
      if (froniusDownSince == 0)
        froniusDownSince = now;

      const bool manualWakeActive =
          (manualWakeUntil != 0) &&
          ((long)(manualWakeUntil - now) > 0);

      if (!manualWakeActive &&
          !froniusAutoBlanked &&
          digitalRead(TFT_PIN) == HIGH &&
          (unsigned long)(now - froniusDownSince) >= FRONIUS_SCREEN_OFF_DELAY_MS) {
        digitalWrite(TFT_PIN, LOW);
        froniusAutoBlanked = true;
        Serial.println("[DISPLAY] OFF - Fronius offline 10 min");
      }
    }

    // Optional automatic screen timeout from the existing configuration.
    if (digitalRead(TFT_PIN) == HIGH && config.ScreenTime != 0) {
      if ((unsigned long)(now - screenOnSince) >=
          (unsigned long)config.ScreenTime * 1000UL) {
        digitalWrite(TFT_PIN, LOW);
        froniusAutoBlanked = false;
      }
    }

    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

#endif
