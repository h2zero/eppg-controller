// Copyright 2021 <Zach Whitehead>
#ifndef INC_SP140_GLOBALS_H_
#define INC_SP140_GLOBALS_H_

byte escData[ESC_DATA_SIZE];
unsigned long cruisedAtMilis = 0; // main
unsigned long transmitted = 0; // esc
unsigned long failed = 0; // esc
bool cruising = false; // main
int prevPotLvl = 0; // throttle, main
int cruisedPotVal = 0; // main
float throttlePWM = 0; // main, display
float batteryPercent = 0; // display, esc
float hours = 0;  // logged flight hours - unused
float wattsHoursUsed = 0; // main, display

uint16_t _volts = 0; // esc
uint16_t _temperatureC = 0; // esc
int16_t _amps = 0; // esc
uint32_t _eRPM = 0; // esc
uint16_t _inPWM = 0; // esc
uint16_t _outPWM = 0; // esc

// ESC Telemetry
float watts = 0; // display, esc, main


// ALTIMETER
float ambientTempC = 0;
float altitudeM = 0;
float aglM = 0;

#endif  // INC_SP140_GLOBALS_H_
