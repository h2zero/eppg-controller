#ifndef INC_EPPG_ESC_H_
#define INC_EPPG_ESC_H_

#ifndef ESC_DISABLED

#ifdef M0_PIO
  #include "../../inc/sp140/m0-config.h"          // device config
#elif RP_PIO
  #include "../../inc/sp140/rp2040-config.h"         // device config
#elif ESP_PLATFORM
  #include "../../inc/esp32/esp32-config.h"
#endif
#include <Servo.h>

class EppgEsc : public Servo {
  byte escData[ESC_DATA_SIZE];
  unsigned long transmitted;
  unsigned long failed;
  uint16_t _volts;
  uint16_t _temperatureC;
  int16_t _amps;
  uint32_t _eRPM;
  uint16_t _inPWM;
  uint16_t _outPWM;

  void serialRead();
  bool enforceFletcher16();
  void parseData();
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

#endif // ESC_DISABLED
#endif // INC_EPPG_ESC_H_
