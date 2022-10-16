#ifndef INC_EPPG_BUTTONS_H_
#define INC_EPPG_BUTTONS_H_

#include <freertos/timers.h>

void   checkButtons(xTimerHandle pxTimer);
void   initButtons();
void   modeSwitch();

#endif