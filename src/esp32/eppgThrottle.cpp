#ifndef EPPG_BLE_HUB

#include <Arduino.h>

#ifdef M0_PIO
  #include "../../inc/sp140/m0-config.h"          // device config
#elif RP_PIO
  #include "../../inc/sp140/rp2040-config.h"         // device config
#elif ESP_PLATFORM
  #include "../../inc/esp32/esp32-config.h"
#endif
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
  if (!armed) return;  // safe

  armedSecs = (millis() - armedAtMilis) / 1000;  // update time while armed

  static int maxPWM = ESC_MAX_PWM;
#ifdef BLE_TEST
  int potRaw = random(0, 4095);
#else
  pot.update();
  int potRaw = pot.getValue();
#endif

  if (cruising) {
    unsigned long cruisingSecs = (millis() - cruisedAtMilis) / 1000;

    if (cruisingSecs >= CRUISE_GRACE && potRaw > POT_SAFE_LEVEL) {
      removeCruise(true);  // deactivate cruise
    } else {
      throttlePWM = mapd(cruisedPotVal, 0, 4095, ESC_MIN_PWM, maxPWM);
    }
  } else {
    // no need to save & smooth throttle etc when in cruise mode (& pot == 0)
    potBuffer.push(potRaw);

    int potLvl = 0;
    for (decltype(potBuffer)::index_t i = 0; i < potBuffer.size(); i++) {
      potLvl += potBuffer[i] / potBuffer.size();  // avg
    }

  // runs ~40x sec
  // 1000 diff in pwm from 0
  // 1000/6/40
    if (deviceData.performance_mode == 0) {  // chill mode
      potLvl = limitedThrottle(potLvl, prevPotLvl, 50);
      maxPWM = 1850;  // 85% interpolated from 1030 to 1990
    } else {
      potLvl = limitedThrottle(potLvl, prevPotLvl, 120);
      maxPWM = ESC_MAX_PWM;
    }
    // mapping val to min and max pwm
    throttlePWM = mapd(potLvl, 0, 4095, ESC_MIN_PWM, maxPWM);
  }

#ifdef EPPG_BLE_HANDHELD
  #ifdef BLE_TEST
  Serial.printf("Set throttle: %d, %s\n", (int)throttlePWM,
                ble.setThrottle(throttlePWM) ? "success" : "failed");
  #else
  // TODO: consider not sending the data if unchanged.
  ble.setThrottle(throttlePWM);
  #endif
#else
  esc.writeMicroseconds(throttlePWM);  // using val as the signal to esc
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

#endif // EPPG_BLE_HUB