// Copyright 2021 <Zach Whitehead>
#ifndef INC_GLOBALS_H_
#define INC_GLOBALS_H_


// Global functions

// main
bool   armSystem();
bool   disarmSystem();
void   handleThrottle();
void   blinkLED(xTimerHandle pxTimer);
bool   throttleSafe();
void   handleArmFail();
void   removeCruise(bool alert);
void   setCruise();
float  getAltitudeM();

#endif  // INC_GLOBALS_H_
