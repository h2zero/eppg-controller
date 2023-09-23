// Copyright 2021 <Zach Whitehead>
#ifndef INC_ESP32_HUB_CONFIG_H_
#define INC_ESP32_HUB_CONFIG_H_

#include "esp32-config.h"

// Arduino Pins
#define BUTTON_TOP    1  // arm/disarm button_top
#define BUZZER_PIN    4   // output for buzzer speaker
#define LED_SW        18  // output for LED

#define SerialESC  Serial0  // ESC UART connection

// SP140
#define ESC_PIN 5

#endif  // INC_ESP32_HUB_CONFIG_H_
