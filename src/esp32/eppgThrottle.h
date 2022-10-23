#ifndef EPPG_BLE_HUB

#ifndef INC_EPPG_THROTTLE_H_
#define INC_EPPG_THROTTLE_H_

#include <ResponsiveAnalogRead.h>
#include <CircularBuffer.h>

class EppgThrottle {
  ResponsiveAnalogRead   pot;
  CircularBuffer<int, 8> potBuffer;
  float                  throttlePWM;
  int                    armedSecs;
  int                    prevPotLvl;
  int                    cruisedPotVal;
  unsigned long          armedAtMilis;
  unsigned long          cruisedAtMilis;
  bool                   cruising;

  int                    limitedThrottle(int current, int last, int threshold);

public:
  EppgThrottle();
  void  setPWM(float val);
  void  handleThrottle();
  bool  safe();
  void  setCruise(bool enable);
  void  setArmed(bool enable);
  int   getArmedSeconds() { return armedSecs; }
  float getPWM() { return throttlePWM; }
  bool  getCruising() { return cruising; }
};

#endif // INC_EPPG_THROTTLE_H_
#endif // EPPG_BLE_HUB