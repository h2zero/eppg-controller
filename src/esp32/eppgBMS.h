#ifndef INC_EPPG_BMS_H_
#define INC_EPPG_BMS_H_

#ifndef BMS_TELEM_DISABLED

#include "../../inc/eppgConfig.h"

class EppgBms : public Servo {
  byte mmsData[BMS_DATA_V2_SIZE];
  uint16_t      _volts;
  uint16_t      _temperatureC;
  int16_t       _amps;
  uint32_t      _eRPM;
  uint16_t      _inPWM;
  uint16_t      _outPWM;

  void serialRead();
  int checkFlectcher16(byte buffer[]);
  void parseData(byte buffer[]);
  void printRawSentence();

public:
  EppgBms();
  void begin();
  void handleTelemetry();
};

#else
class EppgBms {
public:
  void begin() {}
  void handleTelemetry() {}
  void printRawSentence() {}
  bool attach(int pin) { (void)pin; return false; }
  void writeMicroseconds(int pulseUs) {}
};

#endif // BMS_TELEM_DISABLED
#endif // INC_EPPG_BMS_H_
