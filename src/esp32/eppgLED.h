#ifndef INC_EPPG_LED_H_
#define INC_EPPG_LED_H_

#include <Arduino.h>

void   initLEDs();
void   blinkLED(xTimerHandle pxTimer);
void   stopBlinkTimer();
void   startBlinkTimer();
void   setLEDs(byte state);

#endif // INC_EPPG_LED_H_
