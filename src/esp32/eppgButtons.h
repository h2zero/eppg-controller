#ifndef EPPG_BLE_HUB

#ifndef INC_EPPG_BUTTONS_H_
#define INC_EPPG_BUTTONS_H_

#include <freertos/timers.h>

void   checkButtons(xTimerHandle pxTimer);
void   initButtons();
void   modeSwitch();

#endif // INC_EPPG_BUTTONS_H_
#endif // EPPG_BLE_HUB
