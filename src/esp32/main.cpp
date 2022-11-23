// Copyright 2020 <Zach Whitehead>
// OpenPPG
#include <Arduino.h>
#include "../../inc/esp32/esp32-config.h"
#include "../../inc/esp32/structs.h"         // data structs

#ifndef EPPG_BLE_HANDHELD
  #include <Adafruit_BMP3XX.h>     // barometer
#endif

#ifndef EPPG_BLE_HUB
  #include <Adafruit_DRV2605.h>    // haptic controller
  #include <ResponsiveAnalogRead.h>  // smoothing for throttle
#endif

#include <CircularBuffer.h>      // smooth out readings
#include "ble-handheld.h"        // BLE
#include "ble-hub.h"
#include "eppgESC.h"
#include "eppgDisplay.h"
#include "eppgStorage.h"
#include "eppgButtons.h"
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
Adafruit_BMP3XX bmp;
EppgEsc esc;  // Creating a servo class with name of esc
#elif EPPG_BLE_HANDHELD
EppgBLEClient ble;
EppgThrottle throttle;
Adafruit_DRV2605 vibe;
EppgDisplay display;
#endif

CircularBuffer<float, 50> voltageBuffer;
STR_DEVICE_DATA_140_V1 deviceData;
STR_ESC_TELEMETRY_140 telemetryData;

#ifdef M0_PIO
  extEEPROM eep(kbits_64, 1, 64);
#endif

bool armed = false;
float armAltM = 0;

// bmp
static float ambientTempC = 0;
static float altitudeM = 0;
static float aglM = 0;

// local functions
void initBmp();
void initVibe();
void vibrateNotify();
bool runVibe(unsigned int sequence[], int siz);

#pragma message "Warning: OpenPPG software is in beta"

// the setup function runs once when you press reset or power the board
void setup() {
  Serial.begin(115200);

#ifdef USE_TINYUSB
  initWebUSB();
#endif

#if defined(EPPG_BLE_HUB)
  esc.begin();
  setupBleServer();
#endif

#if defined(EPPG_BLE_HANDHELD)
  pinMode(LED_SW, OUTPUT);

  analogReadResolution(12);

  initButtons();
  EEPROM.begin(512);
  refreshDeviceData();

  setupBleClient();
  display.init();
  //modeSwitch();
#endif
}

// main loop - everything runs in threads
void loop() {
  // from WebUSB to both Serial & webUSB
#if defined(USE_TINYUSB) && !defined(BLE_TEST) // causes delay?
  if (!armed) parse_usb_serial();
#endif

#ifdef EPPG_BLE_HUB
  bleServerLoop();
#endif

#ifdef EPPG_BLE_HANDHELD
  bleClientLoop();
#endif
}

// disarm, remove cruise, alert, save updated stats
void disarmSystem() {
#if !defined(EPPG_BLE_HANDHELD)
  armed = false;
  esc.writeMicroseconds(ESC_DISARMED_PWM);
#else
  throttle.setPWM(ESC_DISARMED_PWM);
  armed = !ble.disarm();
  Serial.printf("Disarm: %s\n", !armed ? "success" : "failed");
  //TODO : handle failure
  throttle.setArmed(armed);
  removeCruise(false);
  startBlinkTimer();

  #ifndef BLE_TEST
  u_int16_t disarm_melody[] = { 2093, 1976, 880 };
  unsigned int disarm_vibes[] = { 100, 0 };
  runVibe(disarm_vibes, 3);
  playMelody(disarm_melody, 3);
  display.displayDisarm();
  // update armed_time
  refreshDeviceData();
  deviceData.armed_time += round(throttle.getArmedSeconds() / 60);  // convert to mins
  writeDeviceData();
  #endif

  delay(1000);  // TODO just disable button thread // dont allow immediate rearming
#endif // !defined(EPPG_BLE_HANDHELD)
}

#ifndef EPPG_BLE_HANDHELD
// Start the bmp388 sensor
void initBmp() {
  bmp.begin_I2C();
  bmp.setOutputDataRate(BMP3_ODR_25_HZ);
  bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_2X);
  bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
  bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_15);
}
#endif

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

void handleArmFail() {
  uint16_t arm_fail_melody[] = { 820, 640 };
  playMelody(arm_fail_melody, 2);
}

#ifndef EPPG_BLE_HUB
// initialize the vibration motor
void initVibe() {
  vibe.begin();
  vibe.selectLibrary(1);
  vibe.setMode(DRV2605_MODE_INTTRIG);
  vibrateNotify();
}

bool runVibe(unsigned int sequence[], int siz) {
  if (!ENABLE_VIB) { return false; }

  for (int thisVibe = 0; thisVibe < siz; thisVibe++) {
    vibe.setWaveform(thisVibe, sequence[thisVibe]);
  }
  vibe.go();
  return true;
}

void vibrateNotify() {
  if (!ENABLE_VIB) { return; }

  vibe.setWaveform(0, 15);  // 1 through 117 (see example sketch)
  vibe.setWaveform(1, 0);
  vibe.go();
}
#endif

// get the PPG ready to fly
bool armSystem() {
#if !defined(EPPG_BLE_HANDHELD)
  armed = true;
  esc.writeMicroseconds(ESC_DISARMED_PWM);  // initialize the signal to low
#else
  armed = ble.arm();
  Serial.printf("Arm: %s\n", armed ? "success" : "failed");
  throttle.setArmed(armed);
  stopBlinkTimer();

  #ifndef BLE_TEST
  armAltM = getAltitudeM();
  setLEDs(HIGH);

  uint16_t arm_melody[] = { 1760, 1976, 2093 };
  unsigned int arm_vibes[] = { 70, 33, 0 };
  runVibe(arm_vibes, 3);
  playMelody(arm_melody, 3);
  display.displayArm();
  #endif

#endif // !defined(EPPG_BLE_HANDHELD)
  return true;
}

// convert barometer data to altitude in meters
float getAltitudeM() {
#ifdef EPPG_BLE_HANDHELD
  ambientTempC = ble.getTemp();
  float atmospheric = ble.getBmp() / 100.0F;
  float altitudeM = 44330.0 * (1.0 - pow(atmospheric / deviceData.sea_pressure, 0.1903));
#else
  bmp.performReading();
  ambientTempC = bmp.temperature;
  float altitudeM = bmp.readAltitude(deviceData.sea_pressure);
#endif
  aglM = altitudeM - armAltM;
  return altitudeM;
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
