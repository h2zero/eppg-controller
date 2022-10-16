#ifndef INC_BLE_HANDHELD_H_
#define INC_BLE_HANDHELD_H_

#include <eppgBLE.h>

void   setupBleClient();
void   bleClientLoop();
void   stopBlinkTimer();
void   startBlinkTimer();

#endif