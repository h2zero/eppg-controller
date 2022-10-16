// Copyright 2020 <Zach Whitehead>
// OpenPPG

#include "../../lib/crc.c"       // packet error checking
#ifdef M0_PIO
  #include "../../inc/sp140/m0-config.h"          // device config
#elif RP_PIO
  #include "../../inc/sp140/rp2040-config.h"         // device config
#elif ESP_PLATFORM
  #include "../../inc/esp32/esp32-config.h"
#endif

#include "../../inc/esp32/structs.h"         // data structs
#include <AceButton.h>           // button clicks
#include <Adafruit_BMP3XX.h>     // barometer
#include <Adafruit_DRV2605.h>    // haptic controller
#include <ArduinoJson.h>
#include <CircularBuffer.h>      // smooth out readings
#include <eppgBLE.h>             // BLE
#include <ResponsiveAnalogRead.h>  // smoothing for throttle
#include <Servo.h>               // to control ESCs
#include <SPI.h>
#ifndef ESP_PLATFORM
#include <StaticThreadController.h>
#include <Thread.h>   // run tasks at different intervals
#endif
#include <Wire.h>
#include "display.h"

#ifdef USE_TINYUSB
  #ifdef ESP_PLATFORM
    #if ARDUINO_USB_MODE
      #error USB OTG Mode must be enabled.
    #endif
    #include "USB.h"
  #else
    #include "Adafruit_TinyUSB.h"
  #endif
#endif

#ifdef M0_PIO
  #include <Adafruit_SleepyDog.h>  // watchdog
  #include <extEEPROM.h>  // https://github.com/PaoloP74/extEEPROM
#elif RP_PIO
  // rp2040 specific libraries here
  #include <EEPROM.h>
  #include "hardware/watchdog.h"
  #include "pico/unique_id.h"
#elif ESP_PLATFORM
  #include <EEPROM.h>
#endif

#include "../../inc/esp32/globals.h"  // device config

using namespace ace_button;

EppgDisplay display;
Adafruit_DRV2605 vibe;
Adafruit_BMP3XX bmp;
Servo esc;  // Creating a servo class with name of esc
STR_DEVICE_DATA_140_V1 deviceData;

// USB WebUSB object
#ifdef USE_TINYUSB
  #ifndef ESP_PLATFORM
Adafruit_USBD_WebUSB usb_web;
WEBUSB_URL_DEF(landingPage, 1 /*https*/, "config.openppg.com");
  #else
USBCDC usb_web;
  #endif
#endif

ResponsiveAnalogRead pot(THROTTLE_PIN, false);
AceButton button_top(BUTTON_TOP);
ButtonConfig* buttonConfig = button_top.getButtonConfig();
#ifdef M0_PIO
  extEEPROM eep(kbits_64, 1, 64);
#endif

CircularBuffer<float, 50> voltageBuffer;
CircularBuffer<int, 8> potBuffer;

#ifdef ESP_PLATFORM
xTimerHandle ledBlinkHandle;
xTimerHandle checkButtonsHandle;
#else
Thread ledBlinkThread = Thread();
Thread displayThread = Thread();
Thread throttleThread = Thread();
Thread buttonThread = Thread();
Thread telemetryThread = Thread();
Thread counterThread = Thread();

StaticThreadController<6> threads(&ledBlinkThread, &displayThread, &throttleThread,
                                  &buttonThread, &telemetryThread, &counterThread);
#endif

bool armed = false;
bool use_hub_v2 = true;
float armAltM = 0;
uint32_t armedAtMilis = 0;
uint32_t cruisedAtMilisMilis = 0;
unsigned int armedSecs = 0;
unsigned int last_throttle = 0;

#ifdef EPPG_BLE_SERVER
EppgBLEServer ble;
#elif EPPG_BLE_CLIENT
EppgBLEClient ble;
#endif

#ifdef BLE_LATENCY_TEST
latency_test_t ble_lat_test;
latency_test_t ble_lat_last;
uint32_t disconnect_count;
#endif

#pragma message "Warning: OpenPPG software is in beta"

// the setup function runs once when you press reset or power the board
void setup() {
  Serial.begin(115200);

#ifdef USE_TINYUSB
  #ifdef ESP_PLATFORM
  USB.webUSB(true);
  USB.webUSBURL("https://config.openppg.com");
  USB.begin();
  #else
  usb_web.begin();
  usb_web.setLandingPage(&landingPage);
  usb_web.setLineStateCallback(line_state_callback);
  #endif
#endif

#ifndef ESP_PLATFORM
  SerialESC.begin(ESC_BAUD_RATE);
  SerialESC.setTimeout(ESC_TIMEOUT);

  //Serial.print(F("Booting up (USB) V"));
  //Serial.print(VERSION_MAJOR + "." + VERSION_MINOR);

  pinMode(LED_SW, OUTPUT);   // set up the internal LED2 pin

  analogReadResolution(12);     // M0 family chip provides 12bit resolution
  pot.setAnalogResolution(4096);
  unsigned int startup_vibes[] = { 27, 27, 0 };
  initButtons();

  ledBlinkThread.onRun(blinkLED);
  ledBlinkThread.setInterval(500);

  displayThread.onRun(updateDisplay);
  displayThread.setInterval(250);

  buttonThread.onRun(checkButtons);
  buttonThread.setInterval(5);

  throttleThread.onRun(handleThrottle);
  throttleThread.setInterval(22);

  telemetryThread.onRun(handleTelemetry);
  telemetryThread.setInterval(50);

  counterThread.onRun(trackPower);
  counterThread.setInterval(250);

#ifdef M0_PIO
  Watchdog.enable(5000);
  uint8_t eepStatus = eep.begin(eep.twiClock100kHz);
#elif RP_PIO
  watchdog_enable(5000, 1);
  EEPROM.begin(512);
#endif
  refreshDeviceData();
  setup140();
#ifdef M0_PIO
  Watchdog.reset();
#endif
  initDisplay();
  modeSwitch();

#else // ESP_PLATFORM
  #if defined(EPPG_BLE_SERVER)
  setupBleServer();
  #endif

  #if defined(EPPG_BLE_CLIENT)
  setupBleClient();
  display.init();
  //modeSwitch();
  #endif
#endif // ESP_PLATFORM
}

void setup140() {
  esc.attach(ESC_PIN);
  esc.writeMicroseconds(ESC_DISARMED_PWM);

  initBuzz();
  initBmp();
  getAltitudeM();  // throw away first value
  initVibe();
}

// main loop - everything runs in threads
void loop() {
#ifdef M0_PIO
  Watchdog.reset();
#elif RP_PIO
  watchdog_update();
#endif

  // from WebUSB to both Serial & webUSB
#if defined(USE_TINYUSB) && !defined(BLE_TEST) // causes delay?
  if (!armed && usb_web.available()) parse_usb_serial();
#endif

#ifndef ESP_PLATFORM
  threads.run();
#else
  #ifdef EPPG_BLE_SERVER
  bleServerLoop();
  #endif

  #ifdef EPPG_BLE_CLIENT
  bleClientLoop();
  #endif
#endif // ESP_PLATFORM
}

#ifdef RP_PIO
// set up the second core. Nothing to do for now
void setup1() {}

// automatically runs on the second core of the RP2040
void loop1() {
  if (rp2040.fifo.available() > 0) {
    STR_NOTE noteData;
    uint32_t note_msg = rp2040.fifo.pop();  // get note from fifo queue
    memcpy((uint32_t*)&noteData, &note_msg, sizeof(noteData));
    tone(BUZZER_PIN, noteData.freq);
    delay(noteData.duration);
    noTone(BUZZER_PIN);
  }
}
#endif

#ifdef ESP_PLATFORM
void checkButtons(xTimerHandle pxTimer) {
#else
void checkButtons() {
#endif
  button_top.check();
}

// disarm, remove cruise, alert, save updated stats
void disarmSystem() {
  throttlePWM = ESC_DISARMED_PWM;
#if !defined(EPPG_BLE_CLIENT)
  armed = false;
  esc.writeMicroseconds(ESC_DISARMED_PWM);
#else
  armed = !ble.disarm();
  Serial.printf("Disarm: %s\n", !armed ? "success" : "failed");
#endif
  //Serial.println(F("disarmed"));

#ifndef BLE_TEST
  // reset smoothing
  potBuffer.clear();
  prevPotLvl = 0;

  u_int16_t disarm_melody[] = { 2093, 1976, 880 };
  unsigned int disarm_vibes[] = { 100, 0 };

  removeCruise(false);
#endif

  #ifdef ESP_PLATFORM
  xTimerReset(ledBlinkHandle, portMAX_DELAY);
  #else
  ledBlinkThread.enabled = true;
  #endif

#ifndef BLE_TEST
  runVibe(disarm_vibes, 3);
  playMelody(disarm_melody, 3);
  display.displayDisarm();
  // update armed_time
  refreshDeviceData();
  deviceData.armed_time += round(armedSecs / 60);  // convert to mins
  writeDeviceData();
#endif
  delay(1000);  // TODO just disable button thread // dont allow immediate rearming
}

// The event handler for the the buttons
void handleButtonEvent(AceButton* /* btn */, uint8_t eventType, uint8_t /* st */) {
  switch (eventType) {
  case AceButton::kEventDoubleClicked:
    if (armed) {
      disarmSystem();
    } else if (throttleSafe()) {
      armSystem();
    } else {
      handleArmFail();
    }
    break;
  case AceButton::kEventLongPressed:
    if (armed) {
      if (cruising) {
        removeCruise(true);
      } else {
        setCruise();
      }
    } else {
      // show stats screen?
    }
    break;
  case AceButton::kEventLongReleased:
    break;
  }
}

// inital button setup and config
void initButtons() {
  pinMode(BUTTON_TOP, INPUT_PULLUP);

  buttonConfig->setEventHandler(handleButtonEvent);
  buttonConfig->setFeature(ButtonConfig::kFeatureDoubleClick);
  buttonConfig->setFeature(ButtonConfig::kFeatureLongPress);
  buttonConfig->setFeature(ButtonConfig::kFeatureSuppressAfterDoubleClick);
  buttonConfig->setFeature(ButtonConfig::kFeatureSuppressAfterLongPress);
  buttonConfig->setLongPressDelay(2500);
  buttonConfig->setDoubleClickDelay(600);
}

// read throttle and send to hub
// read throttle
void handleThrottle() {
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

#if defined(EPPG_BLE_CLIENT)
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

// get the PPG ready to fly
bool armSystem() {
  uint16_t arm_melody[] = { 1760, 1976, 2093 };
  unsigned int arm_vibes[] = { 70, 33, 0 };

#if !defined(EPPG_BLE_CLIENT)
  armed = true;
  esc.writeMicroseconds(ESC_DISARMED_PWM);  // initialize the signal to low
#else
  armed = ble.arm();
  Serial.printf("Arm: %s\n", armed ? "success" : "failed");
#endif

#ifdef ESP_PLATFORM
  xTimerStop(ledBlinkHandle, portMAX_DELAY);
#else
  ledBlinkThread.enabled = false;
#endif

#ifndef BLE_TEST
  armedAtMilis = millis();
  armAltM = getAltitudeM();

  setLEDs(HIGH);
  runVibe(arm_vibes, 3);
  playMelody(arm_melody, 3);
  display.displayArm();
#endif
  return true;
}


// Returns true if the throttle/pot is below the safe threshold
bool throttleSafe() {
  pot.update();
  if (pot.getValue() < POT_SAFE_LEVEL) {
    return true;
  }
  return false;
}

// convert barometer data to altitude in meters
float getAltitudeM() {
#ifdef EPPG_BLE_CLIENT
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

void setCruise() {
  // IDEA: fill a "cruise indicator" as long press activate happens
  // or gradually change color from blue to yellow with time
  if (!throttleSafe()) {  // using pot/throttle
    cruisedPotVal = pot.getValue();  // save current throttle val
    cruising = true;
    vibrateNotify();

    uint16_t notify_melody[] = { 900, 900 };
    playMelody(notify_melody, 2);
    display.displaySetCruise();
    cruisedAtMilis = millis();  // start timer
  }
}

void removeCruise(bool alert) {
  cruising = false;
  display.displayRemoveCruise();
  if (alert) {
    vibrateNotify();

    if (ENABLE_BUZ) {
      uint16_t notify_melody[] = { 500, 500 };
      playMelody(notify_melody, 2);
    }
  }
}

unsigned long prevPwrMillis = 0;

void trackPower() {
  unsigned long currentPwrMillis = millis();
  unsigned long msec_diff = (currentPwrMillis - prevPwrMillis);  // eg 0.30 sec
  prevPwrMillis = currentPwrMillis;

  if (armed) {
    wattsHoursUsed += round(watts/60/60*msec_diff)/1000.0;
  }
}
