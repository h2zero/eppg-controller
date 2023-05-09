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
// #define TFT_RST 10
// #define TFT_CS 38
// #define TFT_DC 7
#define TFT_LITE 14

// #define TFT_WIDTH  240 // ST7789 240 x 240 and 240 x 320
// #define TFT_HEIGHT 320 // ST7789 240 x 320
// #define TFT_MISO  MISO
// #define TFT_MOSI  MOSI
// #define TFT_SCLK  SCK

// #define TFT_CS   12  // Chip select control pin
// #define TFT_DC   11  // Data Command control pin
// #define TFT_RST  14  // Reset pin (could connect to Arduino RESET pin)


// #define LOAD_GLCD   // Font 1. Original Adafruit 8 pixel font needs ~1820 bytes in FLASH
// #define LOAD_FONT2  // Font 2. Small 16 pixel high font, needs ~3534 bytes in FLASH, 96 characters
// #define LOAD_FONT4  // Font 4. Medium 26 pixel high font, needs ~5848 bytes in FLASH, 96 characters
// #define LOAD_FONT6  // Font 6. Large 48 pixel font, needs ~2666 bytes in FLASH, only characters 1234567890:-.apm
// #define LOAD_FONT7  // Font 7. 7 segment 48 pixel font, needs ~2438 bytes in FLASH, only characters 1234567890:-.
// #define LOAD_FONT8  // Font 8. Large 75 pixel font needs ~3256 bytes in FLASH, only characters 1234567890:-.
// //#define LOAD_FONT8N // Font 8. Alternative to Font 8 above, slightly narrower, so 3 digits fit a 160 pixel TFT
// #define LOAD_GFXFF  // FreeFonts. Include access to the 48 Adafruit_GFX free fonts FF1 to FF48 and custom fonts

// // Comment out the #define below to stop the SPIFFS filing system and smooth font code being loaded
// // this will save ~20kbytes of FLASH
// #define SMOOTH_FONT
// #define SPI_FREQUENCY  70000000

#endif  // INC_ESP32_CONTROLLER_CONFIG_H_
