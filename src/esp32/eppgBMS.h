#ifndef INC_EPPG_BMS_H_
#define INC_EPPG_BMS_H_

#ifndef BMS_TELEM_DISABLED
#ifdef EPPG_BLE_HUB

#include "../../inc/eppgConfig.h"
#include <HardwareSerial.h>
#include <daly-bms-uart.h>

class EppgBms {
  byte bmsData[ESC_DATA_V2_SIZE];
  uint16_t      _volts;
  uint16_t      _temperatureC;
  int16_t       _amps;
  uint32_t      _eRPM;
  uint16_t      _inPWM;
  uint16_t      _outPWM;

  void printDebug();

public:
  EppgBms();
  void begin();
  void handleBms();
};

#else
class EppgBms {
public:
  void begin() {}
  void handleBms() {}
  void printRawSentence() {}
#endif // EPPG_BLE_HUB
#endif // BMS_TELEM_DISABLED
#endif // INC_EPPG_BMS_H_
