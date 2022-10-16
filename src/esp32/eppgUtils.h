#ifndef INC_EPPG_UTILS_H_
#define INC_EPPG_UTILS_H_

#include <Arduino.h>

void   rebootBootloader();
void   printDeviceData();
String chipId();
double mapd(double x, double in_min, double in_max, double out_min, double out_max);

#endif