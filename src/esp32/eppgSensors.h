#ifndef INC_EPPG_SENSORS_H_
#define INC_EPPG_SENSORS_H_

#ifndef EPPG_BLE_HANDHELD
#include <Adafruit_BMP3XX.h>     // barometer
#endif

void  initBmp();
float getAltitudeM();
float getPressure();
float getAmbientTempC();

#endif // INC_EPPG_SENSORS_H_
