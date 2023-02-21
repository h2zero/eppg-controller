#ifndef INC_EPPG_SENSORS_H_
#define INC_EPPG_SENSORS_H_

#ifndef EPPG_BLE_HANDHELD
#include <Adafruit_BMP3XX.h>     // barometer
#include <SparkFun_u-blox_GNSS_Arduino_Library.h>
#endif

void  initBmp();
float getAltitudeM();
float getPressure();
float getAmbientTempC();

void initGps();
void debugGps();

#endif // INC_EPPG_SENSORS_H_
