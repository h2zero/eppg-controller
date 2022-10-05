// Copyright 2021 <Zach Whitehead>
#ifndef INC_ESP32_CONFIG_H_
#define INC_ESP32_CONFIG_H_

#include "shared-config.h"

// Arduino Pins
#define BUTTON_TOP    5  // arm/disarm button_top
#define BUTTON_SIDE   7   // secondary button_top
#define BUZZER_PIN    4   // output for buzzer speaker
#define LED_SW        10  // output for LED
#define THROTTLE_PIN  A0  // throttle pot input

#define SerialESC  Serial1  // ESC UART connection

// SP140
#define POT_PIN A0
#define TFT_RST 5
#define TFT_CS 13
#define TFT_DC 11
#define TFT_LITE 25
#define ESC_PIN 14
#define ENABLE_VIB            false    // enable vibration

#endif  // INC_ESP32_CONFIG_H_
