// Copyright 2020 <Zach Whitehead>
// OpenPPG
#include <Arduino.h>
#include "../../inc/eppgConfig.h"
#include "../../inc/esp32/structs.h"         // data structs

#ifndef EPPG_BLE_HANDHELD
  #include "eppgSensors.h"     // barometer
#endif

#ifndef EPPG_BLE_HUB
  #include <ResponsiveAnalogRead.h>  // smoothing for throttle
  #include "eppgLED.h"
#endif

#include "ble-handheld.h"        // BLE
#include "ble-hub.h"
#include "eppgESC.h"
#include "eppgBMS.h"
#include "eppgDisplay.h"
#include "eppgStorage.h"
//#include "eppgButtons.h"
#include "eppgUtils.h"
#include "eppgBuzzer.h"
#include "eppgThrottle.h"

#ifdef USE_TINYUSB
  #include "eppgUSB.h"
#endif

#include <EEPROM.h>
#include "../../inc/esp32/globals.h"  // device config

// globally available
#ifdef EPPG_BLE_HUB
EppgBLEServer ble;
EppgEsc esc;  // Creating a servo class with name of esc
EppgBms bms;
#elif EPPG_BLE_HANDHELD
EppgBLEClient ble;
EppgThrottle throttle;
EppgDisplay display;
#endif

STR_DEVICE_DATA_140_V1 deviceData;
STR_BMS_DATA bmsData;
bool armed = false;
bool hub_armed = false;

#pragma message "Warning: OpenPPG software is in beta"

// the setup function runs once when you press reset or power the board
void setup() {
  Serial.begin(115200);

#ifdef USE_TINYUSB
  initWebUSB();
#endif

#if defined(EPPG_BLE_HUB)
  esc.begin();
  //bms.begin();

  // initBmp(); // ideally call init sensors
  setupBleServer();
#endif

#if defined(EPPG_BLE_HANDHELD)
  pinMode(LED_SW, OUTPUT);
  pinMode(ARM_SW, INPUT_PULLUP);

  analogReadResolution(12);

  //initButtons();
  //EEPROM.begin(512);
  //refreshDeviceData();

  setupBleClient();
  //display.init();
  //initLEDs();
  throttle.begin();
  //modeSwitch();
#endif
}

// main loop - everything runs in threads
void loop() {
  // from WebUSB to both Serial & webUSB
#if defined(USE_TINYUSB) && !defined(BLE_TEST) // causes delay?
 // if (!armed) parse_usb_serial();
#endif

#ifdef EPPG_BLE_HUB
  bleServerLoop();
#endif

#ifdef EPPG_BLE_HANDHELD
  bleClientLoop();
  //display.update();
  int armSwitchState = digitalRead(ARM_SW);
  Serial.printf("Arm switch: %d\n", armSwitchState);
  if (armSwitchState == HIGH && !armed) {
    if (throttle.safe()) {
      Serial.println("Armed from switch");
      armSystem();
    } else {
      //handleArmFail();
    }
  } else if (armSwitchState == LOW) {
    //armed = false;
    // Serial.println("Disarmed from switch");
    disarmSystem();
  }
  Serial.printf("Armed: %d\n", armed);
  delay(200);

#endif
}

// disarm, remove cruise, alert, save updated stats
bool disarmSystem() {
  armed = false;
  Serial.println("Disarmed");

#if defined(EPPG_BLE_HANDHELD)
  //armed = false;
  hub_armed = ble.disarm();

#else
#endif
  return true;
}

void handleArmFail() {
  Serial.println("Arm failed");
  //uint16_t arm_fail_melody[] = { 820, 640 };
  //playMelody(arm_fail_melody, 2);
}

// get the PPG ready to fly
bool armSystem() {
  Serial.println("Arming");
  armed = true;

#if !defined(EPPG_BLE_HANDHELD)
  //esc.writeMicroseconds(ESC_DISARMED_PWM);  // initialize the signal to low
#else
  hub_armed = ble.arm();
  Serial.printf("Arm: %s\n", armed ? "success" : "failed");
  // if (!hub_armed) {
  //   //handleArmFail();
  //   return false;
  // }
  // throttle.setArmed(armed);
  //stopBlinkTimer();

  #ifndef BLE_TEST
  //display.displayArm();
  #endif

#endif // !defined(EPPG_BLE_HANDHELD)
  return true;
}

#ifndef EPPG_BLE_HUB
void setCruise() {
  // IDEA: fill a "cruise indicator" as long press activate happens
  // or gradually change color from blue to yellow with time
  if (!throttle.safe()) {  // using pot/throttle
    throttle.setCruise(true);
    vibrateNotify();

    uint16_t notify_melody[] = { 900, 900 };
    playMelody(notify_melody, 2);
    display.displaySetCruise();
  }
}

void removeCruise(bool alert) {
  throttle.setCruise(false);
  display.displayRemoveCruise();
  if (alert) {
    vibrateNotify();

    if (ENABLE_BUZ) {
      uint16_t notify_melody[] = { 500, 500 };
      playMelody(notify_melody, 2);
    }
  }
}
#endif
