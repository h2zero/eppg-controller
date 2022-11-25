#include "eppgLED.h"
#include "../../inc/eppgConfig.h"

static xTimerHandle ledBlinkHandle;

void stopBlinkTimer() {
  xTimerStop(ledBlinkHandle, portMAX_DELAY);
}

void startBlinkTimer() {
  xTimerReset(ledBlinkHandle, portMAX_DELAY);
}

void setLEDs(byte state) {
  // digitalWrite(LED_2, state);
  // digitalWrite(LED_3, state);
  digitalWrite(LED_SW, state);
}

// toggle LEDs
void blinkLED(xTimerHandle pxTimer) {
  byte ledState = !digitalRead(LED_SW);
  setLEDs(ledState);
}

void initLEDs() {
  // 500ms timer to blink the LED.
  ledBlinkHandle = xTimerCreate("blinkLED", pdMS_TO_TICKS(500), pdTRUE, NULL, blinkLED);
  xTimerReset(ledBlinkHandle, portMAX_DELAY);
}