// Copyright 2021 <Zach Whitehead>
#ifndef INC_SP140_GLOBALS_H_
#define INC_SP140_GLOBALS_H_


unsigned long cruisedAtMilis = 0; // main
bool cruising = false; // main
int prevPotLvl = 0; // throttle, main
int cruisedPotVal = 0; // main
float throttlePWM = 0; // main, display
float batteryPercent = 0; // display, esc
float hours = 0;  // logged flight hours - unused
float wattsHoursUsed = 0; // main, display

// ESC Telemetry
float watts = 0; // display, esc, main


// ALTIMETER
float ambientTempC = 0;
float altitudeM = 0;
float aglM = 0;

#endif  // INC_SP140_GLOBALS_H_
