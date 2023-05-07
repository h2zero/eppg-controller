// Copyright 2021 <Zach Whitehead>
#ifndef INC_ESP32_CONTROLLER_CONFIG_H_
#define INC_ESP32_CONTROLLER_CONFIG_H_

#include "esp32-config.h"

// Arduino Pins
#define BUTTON_TOP    1  // arm/disarm button_top
#define BUZZER_PIN    4   // output for buzzer speaker
#define LED_SW        18  // output for LED
#define THROTTLE_PIN  3  // throttle pot input

// SP140
#define TFT_RST 10
#define TFT_CS 38
#define TFT_DC 7
#define TFT_LITE 14

#endif  // INC_ESP32_CONTROLLER_CONFIG_H_
