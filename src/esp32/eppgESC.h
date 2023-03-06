#ifndef INC_EPPG_ESC_H_
#define INC_EPPG_ESC_H_

#ifndef ESC_TELEM_DISABLED

#include "../../inc/eppgConfig.h"
#include <Servo.h>

class EppgEsc : public Servo {
  byte escData[ESC_DATA_V2_SIZE];
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
  EppgEsc();
  void begin();
  void handleTelemetry();
};

#else
class EppgEsc {
public:
  void begin() {}
  void handleTelemetry() {}
  void printRawSentence() {}
  bool attach(int pin) { (void)pin; return false; }
  void writeMicroseconds(int pulseUs) {}
};

#endif // ESC_TELEM_DISABLED
#endif // INC_EPPG_ESC_H_
