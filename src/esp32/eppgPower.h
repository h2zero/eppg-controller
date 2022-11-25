#ifndef INC_EPPG_POWER_H_
#define INC_EPPG_POWER_H_

void   trackPower();
void   trackPowerTaskStart();
float  getBatteryVoltSmoothed();
float  getBatteryPercent(float voltage);
float  getWattHoursUsed();
void   setWatts(float val);
float  getWatts();
void   pushVoltage(float volts);

#endif