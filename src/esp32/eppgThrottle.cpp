#ifndef EPPG_BLE_HUB

#include <Arduino.h>

#include "../../inc/eppgConfig.h"
#include "../../inc/esp32/globals.h"
#include "../../inc/esp32/structs.h"
#include "eppgThrottle.h"
#include "eppgUtils.h"

#ifdef EPPG_BLE_HANDHELD
#include <eppgBLE.h>
extern EppgBLEClient ble;
#else
#include "eppgESC.h"
extern EppgEsc esc;
#endif

extern STR_DEVICE_DATA_140_V1 deviceData;
extern bool armed;

void handleThrottleTask(void * param) {
  EppgThrottle *throttle = (EppgThrottle*)param;
  for (;;) {
    throttle->handleThrottle();
#ifdef BLE_TEST
    delay(100);
#else
    delay(22); // Update throttle every 22ms
#endif
  }

  vTaskDelete(NULL); // should never reach this
}

EppgThrottle::EppgThrottle()
: pot(THROTTLE_PIN, false),
  potBuffer(),
  throttlePWM(0),
  armedSecs(0),
  prevPotLvl(0),
  cruisedPotVal(0),
  armedAtMilis(0),
  cruisedAtMilis(0),
  cruising(false) {
  pot.setAnalogResolution(4096);
}

void EppgThrottle::setPWM(float val) {
  throttlePWM = val;
}

// read throttle and send to hub
// read throttle
void EppgThrottle::handleThrottle() {
  //TODO remove: if (!armed) return;  // safe
  static int maxPWM = ESC_MAX_PWM;
#ifdef BLE_TEST
  int potRaw = random(0, 4095);
#else
  pot.update();
  int potRaw = pot.getValue();
#endif

throttlePWM = mapd(potRaw, 0, 4095, ESC_MIN_PWM, maxPWM);
Serial.printf("potRaw: %d, throttlePWM: %f\n", potRaw, throttlePWM);

#ifdef EPPG_BLE_HANDHELD
  // TODO: consider not sending the data if unchanged.
  ble.setThrottle(throttlePWM);
#else
  //esc.writeMicroseconds(throttlePWM);  // using val as the signal to esc
#endif
}

// throttle easing function based on threshold/performance mode
int EppgThrottle::limitedThrottle(int current, int last, int threshold) {
  if (current - last >= threshold) {  // accelerating too fast. limit
    int limitedThrottle = last + threshold;
    prevPotLvl = limitedThrottle;  // save for next time
    return limitedThrottle;
  } else if (last - current >= threshold * 2) {  // decelerating too fast. limit
    int limitedThrottle = last - threshold * 2;  // double the decel vs accel
    prevPotLvl = limitedThrottle;  // save for next time
    return limitedThrottle;
  }
  prevPotLvl = current;
  return current;
}

// Returns true if the throttle/pot is below the safe threshold
bool EppgThrottle::safe() {
  pot.update();
  if (pot.getValue() < POT_SAFE_LEVEL) {
    return true;
  }
  return false;
}

void EppgThrottle::setCruise(bool enable) {
  cruisedPotVal = pot.getValue();  // save current throttle val
  cruising = true;
  cruisedAtMilis = millis();  // start timer
}

void EppgThrottle::setArmed(bool enable) {
  if (enable) {
    armedAtMilis = millis();
  } else {
    potBuffer.clear();
    prevPotLvl = 0;
  }
}

void EppgThrottle::begin() {
  xTaskCreate(handleThrottleTask, "handleThrottle", 5000, this, 1, NULL);
}

#endif // EPPG_BLE_HUB
