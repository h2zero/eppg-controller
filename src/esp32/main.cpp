// Copyright 2020 <Zach Whitehead>
// OpenPPG
#include <Arduino.h>
#ifdef M0_PIO
  #include "../../inc/sp140/m0-config.h"          // device config
#elif RP_PIO
  #include "../../inc/sp140/rp2040-config.h"         // device config
#elif ESP_PLATFORM
  #include "../../inc/esp32/esp32-config.h"
  #include "../../inc/esp32/structs.h"         // data structs
#endif

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

#ifndef ESP_PLATFORM
Thread ledBlinkThread = Thread();
Thread displayThread = Thread();
Thread throttleThread = Thread();
Thread buttonThread = Thread();
Thread telemetryThread = Thread();
Thread counterThread = Thread();

StaticThreadController<6> threads(&ledBlinkThread, &displayThread, &throttleThread,
                                  &buttonThread, &telemetryThread, &counterThread);
#endif

// globally available
#ifdef EPPG_BLE_HUB
EppgBLEServer ble;
#elif EPPG_BLE_HANDHELD
EppgBLEClient ble;
#endif

EppgThrottle throttle;
CircularBuffer<float, 50> voltageBuffer;
EppgDisplay display;
Adafruit_DRV2605 vibe;
Adafruit_BMP3XX bmp;
EppgEsc esc;  // Creating a servo class with name of esc
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
  esc.begin();

#ifdef USE_TINYUSB
  initWebUSB();
#endif

#ifndef ESP_PLATFORM

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
  #if defined(EPPG_BLE_HUB)
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
  if (!armed) parse_usb_serial();
#endif

#ifndef ESP_PLATFORM
  threads.run();
#else
  #ifdef EPPG_BLE_HUB
  bleServerLoop();
  #endif

  #ifdef EPPG_BLE_HANDHELD
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

// disarm, remove cruise, alert, save updated stats
void disarmSystem() {
#if !defined(EPPG_BLE_HANDHELD)
  armed = false;
  esc.writeMicroseconds(ESC_DISARMED_PWM);
#else
  throttle.setPWM(ESC_DISARMED_PWM);
  armed = !ble.disarm();
  Serial.printf("Disarm: %s\n", !armed ? "success" : "failed");
#endif
  //Serial.println(F("disarmed"));

#ifndef BLE_TEST
  throttle.setArmed(false);

  u_int16_t disarm_melody[] = { 2093, 1976, 880 };
  unsigned int disarm_vibes[] = { 100, 0 };

  removeCruise(false);
#endif

  #ifdef ESP_PLATFORM
  startBlinkTimer();
  #else
  ledBlinkThread.enabled = true;
  #endif

#ifndef BLE_TEST
  runVibe(disarm_vibes, 3);
  playMelody(disarm_melody, 3);
  display.displayDisarm();
  // update armed_time
  refreshDeviceData();
  deviceData.armed_time += round(throttle.getArmedSeconds() / 60);  // convert to mins
  writeDeviceData();
#endif
  delay(1000);  // TODO just disable button thread // dont allow immediate rearming
}

// Start the bmp388 sensor
void initBmp() {
  bmp.begin_I2C();
  bmp.setOutputDataRate(BMP3_ODR_25_HZ);
  bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_2X);
  bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
  bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_15);
}

// initialize the vibration motor
void initVibe() {
  vibe.begin();
  vibe.selectLibrary(1);
  vibe.setMode(DRV2605_MODE_INTTRIG);
  vibrateNotify();
}

void setLEDs(byte state) {
  // digitalWrite(LED_2, state);
  // digitalWrite(LED_3, state);
  digitalWrite(LED_SW, state);
}

// toggle LEDs
#ifdef ESP_PLATFORM
void blinkLED(xTimerHandle pxTimer) {
#else
void blinkLED() {
#endif
  byte ledState = !digitalRead(LED_SW);
  setLEDs(ledState);
}

bool runVibe(unsigned int sequence[], int siz) {
  if (!ENABLE_VIB) { return false; }

  for (int thisVibe = 0; thisVibe < siz; thisVibe++) {
    vibe.setWaveform(thisVibe, sequence[thisVibe]);
  }
  vibe.go();
  return true;
}

void handleArmFail() {
  uint16_t arm_fail_melody[] = { 820, 640 };
  playMelody(arm_fail_melody, 2);
}

void vibrateNotify() {
  if (!ENABLE_VIB) { return; }

  vibe.setWaveform(0, 15);  // 1 through 117 (see example sketch)
  vibe.setWaveform(1, 0);
  vibe.go();
}

// get the PPG ready to fly
bool armSystem() {
  uint16_t arm_melody[] = { 1760, 1976, 2093 };
  unsigned int arm_vibes[] = { 70, 33, 0 };

#if !defined(EPPG_BLE_HANDHELD)
  armed = true;
  esc.writeMicroseconds(ESC_DISARMED_PWM);  // initialize the signal to low
#else
  armed = ble.arm();
  Serial.printf("Arm: %s\n", armed ? "success" : "failed");
#endif

#ifdef ESP_PLATFORM
  stopBlinkTimer();
#else
  ledBlinkThread.enabled = false;
#endif

#ifndef BLE_TEST
  throttle.setArmed(true);
  armAltM = getAltitudeM();

  setLEDs(HIGH);
  runVibe(arm_vibes, 3);
  playMelody(arm_melody, 3);
  display.displayArm();
#endif
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

