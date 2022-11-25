#ifndef INC_EPPG_DISPLAY_H_
#define INC_EPPG_DISPLAY_H_

#ifndef DISPLAY_DISABLED
#include <Adafruit_ST7735.h>

class EppgDisplay {
  Adafruit_ST7735 display;
  float           prevBatteryPercent;
  int             page;
  uint16_t        bottom_bg_color;
  bool            screen_wiped;
  bool            batteryFlag;
  float           prevVolts;
  float           prevAmps;
  bool            throttledFlag;
  bool            throttled;
  unsigned long   throttledAtMillis;
  unsigned int    throttleSecs;
  float           minutes;
  float           prevMinutes;
  float           seconds;
  float           prevSeconds;
  float           prevKilowatts;
  float           prevKwh;
  float           lastAltM;
  float           armAltM;

  void     displayMeta();
  void     dispValue(float value, float &prevVal, int maxDigits, int precision,
                     int x, int y, int textSize, int textColor, int background);
  uint16_t batt2color(int percentage);
  void     handleFlightTime();
  void     displayAlt();
  void     addVSpace();
  void     displayTime(int val);
  void     displayTime(int val, int x, int y, uint16_t bg_color);
  void     displayPage0();
  void     displayPage1();
  String   convertToDigits(byte digits);

public:
  EppgDisplay();

  void   init();
  void   reset();
  void   update();
  void   displayMessage(const char *message);
  void   displayVersions();
  void   displayBootLoader();
  int    nextPage();
  void   displaySetCruise();
  void   displayRemoveCruise();
  void   displayDisarm();
  void   displayArm();
};

#else // DISPLAY_DISABLED
class EppgDisplay {
public:
  void   init() {}
  void   reset() {}
  void   update() {}
  void   displayMessage(const char *message) {(void)message; return;}
  void   displayBootLoader(){}
  void   displaySetCruise(){}
  void   displayRemoveCruise(){}
  void   displayDisarm() {}
  void   displayArm() {}
};

#endif // DISPLAY_DISABLED
#endif // INC_EPPG_DISPLAY_H_