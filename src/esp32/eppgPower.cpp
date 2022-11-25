// Copyright 2021 <Zach Whitehead>
// OpenPPG
#include <Arduino.h>
#include <CircularBuffer.h>
#include "../../inc/esp32/globals.h"
#include "eppgPower.h"
#include "eppgUtils.h"

extern bool armed;

static CircularBuffer<float, 50> voltageBuffer;
static unsigned long prevPwrMillis = 0;
static float wattsHoursUsed = 0;
static float watts = 0;

// simple set of data points from load testing
// maps voltage to battery percentage
float getBatteryPercent(float voltage) {
  float battPercent = 0;

  if (voltage > 94.8) {
    battPercent = mapd(voltage, 94.8, 99.6, 90, 100);
  } else if (voltage > 93.36) {
    battPercent = mapd(voltage, 93.36, 94.8, 80, 90);
  } else if (voltage > 91.68) {
    battPercent = mapd(voltage, 91.68, 93.36, 70, 80);
  } else if (voltage > 89.76) {
    battPercent = mapd(voltage, 89.76, 91.68, 60, 70);
  } else if (voltage > 87.6) {
    battPercent = mapd(voltage, 87.6, 89.76, 50, 60);
  } else if (voltage > 85.2) {
    battPercent = mapd(voltage, 85.2, 87.6, 40, 50);
  } else if (voltage > 82.32) {
    battPercent = mapd(voltage, 82.32, 85.2, 30, 40);
  } else if (voltage > 80.16) {
    battPercent = mapd(voltage, 80.16, 82.32, 20, 30);
  } else if (voltage > 78) {
    battPercent = mapd(voltage, 78, 80.16, 10, 20);
  } else if (voltage > 60.96) {
    battPercent = mapd(voltage, 60.96, 78, 0, 10);
  }
  return constrain(battPercent, 0, 100);
}


// inspired by https://github.com/rlogiacco/BatterySense/
// https://www.desmos.com/calculator/7m9lu26vpy
uint8_t battery_sigmoidal(float voltage, uint16_t minVoltage, uint16_t maxVoltage) {
  uint8_t result = 105 - (105 / (1 + pow(1.724 * (voltage - minVoltage)/(maxVoltage - minVoltage), 5.5)));
  return result >= 100 ? 100 : result;
}

void trackPower() {
  unsigned long currentPwrMillis = millis();
  unsigned long msec_diff = (currentPwrMillis - prevPwrMillis);  // eg 0.30 sec
  prevPwrMillis = currentPwrMillis;

  if (armed) {
    wattsHoursUsed += round(watts/60/60*msec_diff)/1000.0;
  }
}

// ring buffer for voltage readings
float getBatteryVoltSmoothed() {
  float avg = 0.0;

  if (voltageBuffer.isEmpty()) { return avg; }

  using index_t = decltype(voltageBuffer)::index_t;
  for (index_t i = 0; i < voltageBuffer.size(); i++) {
    avg += voltageBuffer[i] / voltageBuffer.size();
  }
  return avg;
}

float getWattHoursUsed() {
  return wattsHoursUsed;
}

void setWatts(float val) {
  watts = val;
}

float getWatts() {
  return watts;
}

void pushVoltage(float volts) {
  voltageBuffer.push(volts);
}

void trackPowerTask(void * parameter) {
  for (;;) {
    trackPower();
    delay(250);
  }

  vTaskDelete(NULL); // should never reach this
}

void trackPowerTaskStart() {
  xTaskCreate(trackPowerTask, "trackPower", 5000, NULL, 1, NULL);
}