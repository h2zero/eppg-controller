#include <Arduino.h>
#include "eppgDisplay.h"

#ifndef DISPLAY_DISABLED

#ifdef M0_PIO
  #include "../../inc/sp140/m0-config.h"          // device config
#elif RP_PIO
  #include "../../inc/sp140/rp2040-config.h"         // device config
#elif ESP_PLATFORM
  #include "../../inc/esp32/esp32-config.h"
#endif
#include <TimeLib.h>  // convert time to hours mins etc
#include <Fonts/FreeSansBold12pt7b.h>
#include "../../inc/esp32/structs.h"
#include "../../inc/esp32/globals.h"
#include "eppgPower.h"
#include "eppgThrottle.h"

extern bool armed;
extern float armAltM;
extern STR_ESC_TELEMETRY_140 telemetryData;
extern STR_DEVICE_DATA_140_V1 deviceData;
extern EppgThrottle throttle;

#define LAST_PAGE 1  // starts at 0
#define BLACK                 ST77XX_BLACK
#define WHITE                 ST77XX_WHITE
#define GREEN                 ST77XX_GREEN
#define YELLOW                ST77XX_YELLOW
#define RED                   ST77XX_RED
#define BLUE                  ST77XX_BLUE
#define ORANGE                ST77XX_ORANGE
#define PURPLE                0x780F

#define DEFAULT_BG_COLOR      WHITE
#define ARMED_BG_COLOR        ST77XX_CYAN
#define CRUISE_BG_COLOR       YELLOW

#define DIGIT_ARRAY_SIZE      7

EppgDisplay::EppgDisplay()
:display(Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST)),
 prevBatteryPercent(0),
 page(0),
 bottom_bg_color(DEFAULT_BG_COLOR),
 screen_wiped(false),
 batteryFlag(true),
 prevVolts(0),
 prevAmps(0),
 throttledFlag(true),
 throttled(false),
 throttledAtMillis(0),
 throttleSecs(0),
 minutes(0),
 prevMinutes(0),
 seconds(0),
 prevSeconds(0),
 prevKilowatts(0),
 prevKwh(0),
 lastAltM(0) {
}

void EppgDisplay::init() {
  display.initR(INITR_BLACKTAB);  // Init ST7735S chip, black tab

  pinMode(TFT_LITE, OUTPUT);
  reset();
  displayMeta();
  digitalWrite(TFT_LITE, HIGH);  // Backlight on
  delay(2500);
}

void EppgDisplay::reset() {
  display.fillScreen(DEFAULT_BG_COLOR);
  display.setTextColor(BLACK);
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextWrap(true);

  display.setRotation(deviceData.screen_rotation);  // 1=right hand, 3=left hand
}

void EppgDisplay::displayMeta() {
  display.setFont(&FreeSansBold12pt7b);
  display.setTextColor(BLACK);
  display.setCursor(25, 30);
  display.println("OpenPPG");
  display.setFont();
  display.setTextSize(2);
  display.setCursor(60, 60);
  display.print("v" + String(VERSION_MAJOR) + "." + String(VERSION_MINOR));
#ifdef RP_PIO
  display.print("R");
#endif
  display.setCursor(54, 90);
  displayTime(deviceData.armed_time);
}

void EppgDisplay::displayTime(int val) {
  int minutes = val / 60;  // numberOfMinutes(val);
  int seconds = numberOfSeconds(val);

  display.print(convertToDigits(minutes));
  display.print(":");
  display.print(convertToDigits(seconds));
}

// TODO (bug) rolls over at 99mins
void EppgDisplay::displayTime(int val, int x, int y, uint16_t bg_color) {
  // displays number of minutes and seconds (since armed and throttled)
  display.setCursor(x, y);
  display.setTextSize(2);
  display.setTextColor(BLACK);
  minutes = val / 60;
  seconds = numberOfSeconds(val);
  if (minutes < 10) {
    display.setCursor(x, y);
    display.print("0");
  }
  dispValue(minutes, prevMinutes, 2, 0, x, y, 2, BLACK, bg_color);
  display.setCursor(x+24, y);
  display.print(":");
  display.setCursor(x+36, y);
  if (seconds < 10) {
    display.print("0");
  }
  dispValue(seconds, prevSeconds, 2, 0, x+36, y, 2, BLACK, bg_color);
}

// display hidden page (firmware version and total armed time)
void EppgDisplay::displayVersions() {
  display.setTextSize(2);
  display.print(F("v"));
  display.print(VERSION_MAJOR);
  display.print(F("."));
  display.println(VERSION_MINOR);
  addVSpace();
  display.setTextSize(2);
  displayTime(deviceData.armed_time);
  display.print(F(" h:m"));
  // addVSpace();
  // display.print(chipId()); // TODO: trim down
}

// display hidden page (firmware version and total armed time)
void EppgDisplay::displayMessage(const char *message) {
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.println(message);
}

/**
 * For digital time display - prints leading 0
 *
 * @param digits number to be converted to a string.
 * @return string `12`, or 07 if `digits` is less than 10.
 */
String EppgDisplay::convertToDigits(byte digits) {
  String digits_string = "";
  if (digits < 10) digits_string.concat("0");
  digits_string.concat(digits);
  return digits_string;
}

void EppgDisplay::addVSpace() {
  display.setTextSize(1);
  display.println(" ");
}

//**************************************************************************************//
//  Helper function to print values without flashing numbers due to slow screen refresh.
//  This function only re-draws the digit that needs to be updated.
//    BUG:  If textColor is not constant across multiple uses of this function,
//          weird things happen.
//**************************************************************************************//
void EppgDisplay::dispValue(float value, float &prevVal, int maxDigits, int precision,
                            int x, int y, int textSize, int textColor, int background) {
  int numDigits = 0;
  char prevDigit[DIGIT_ARRAY_SIZE] = {};
  char digit[DIGIT_ARRAY_SIZE] = {};
  String prevValTxt = String(prevVal, precision);
  String valTxt = String(value, precision);
  prevValTxt.toCharArray(prevDigit, maxDigits+1);
  valTxt.toCharArray(digit, maxDigits+1);

  // COUNT THE NUMBER OF DIGITS THAT NEED TO BE PRINTED:
  for(int i=0; i<maxDigits; i++){
    if (digit[i]) {
      numDigits++;
    }
  }

  display.setTextSize(textSize);
  display.setCursor(x, y);

  // PRINT LEADING SPACES TO RIGHT-ALIGN:
  display.setTextColor(background);
  for(int i=0; i<(maxDigits-numDigits); i++){
    display.print(static_cast<char>(218));
  }
  display.setTextColor(textColor);

  // ERASE ONLY THE NESSESARY DIGITS:
  for (int i=0; i<numDigits; i++) {
    if (digit[i]!=prevDigit[i]) {
      display.setTextColor(background);
      display.print(static_cast<char>(218));
    } else {
      display.setTextColor(textColor);
      display.print(digit[i]);
    }
  }

  // BACK TO THE BEGINNING:
  display.setCursor(x, y);

  // ADVANCE THE CURSOR TO THE PROPER LOCATION:
  display.setTextColor(background);
  for (int i=0; i<(maxDigits-numDigits); i++) {
    display.print(static_cast<char>(218));
  }
  display.setTextColor(textColor);

  // PRINT THE DIGITS THAT NEED UPDATING
  for(int i=0; i<numDigits; i++){
    display.print(digit[i]);
  }

  prevVal = value;
}

// maps battery percentage to a display color
uint16_t EppgDisplay::batt2color(int percentage) {
  if (percentage >= 30) {
    return GREEN;
  } else if (percentage >= 15) {
    return YELLOW;
  }
  return RED;
}

void EppgDisplay::update() {
  if (!screen_wiped) {
    display.fillScreen(WHITE);
    screen_wiped = true;
  }
  //Serial.print("v: ");
  //Serial.println(volts);

  displayPage0();
  //dispValue(kWatts, prevKilowatts, 4, 1, 10, /*42*/55, 2, BLACK, DEFAULT_BG_COLOR);
  //display.print("kW");

  display.setTextColor(BLACK);
  float avgVoltage = getBatteryVoltSmoothed();
  float batteryPercent = getBatteryPercent(avgVoltage);  // multi-point line
  // change battery color based on charge
  int batt_width = map((int)batteryPercent, 0, 100, 0, 108);
  display.fillRect(0, 0, batt_width, 36, batt2color(batteryPercent));

  if (avgVoltage < BATT_MIN_V) {
    if (batteryFlag) {
      batteryFlag = false;
      display.fillRect(0, 0, 108, 36, DEFAULT_BG_COLOR);
    }
    display.setCursor(12, 3);
    display.setTextSize(2);
    display.setTextColor(RED);
    display.println("BATTERY");

    if ( avgVoltage < 10 ) {
      display.print(" ERROR");
    } else {
      display.print(" DEAD");
    }
  } else {
    batteryFlag = true;
    display.fillRect(map(batteryPercent, 0,100, 0,108), 0, map(batteryPercent, 0,100, 108,0), 36, DEFAULT_BG_COLOR);
  }
  // cross out battery box if battery is dead
  if (batteryPercent <= 5) {
    display.drawLine(0, 1, 106, 36, RED);
    display.drawLine(0, 0, 108, 36, RED);
    display.drawLine(1, 0, 110, 36, RED);
  }
  dispValue(batteryPercent, prevBatteryPercent, 3, 0, 108, 10, 2, BLACK, DEFAULT_BG_COLOR);
  display.print("%");

  // battery shape end
  //display.fillRect(102, 0, 6, 9, BLACK);
  //display.fillRect(102, 27, 6, 10, BLACK);

  display.fillRect(0, 36, 160, 1, BLACK);
  display.fillRect(108, 0, 1, 36, BLACK);
  display.fillRect(0, 92, 160, 1, BLACK);

  displayAlt();

  //dispValue(ambientTempF, prevAmbTempF, 3, 0, 10, 100, 2, BLACK, DEFAULT_BG_COLOR);
  //display.print("F");

  handleFlightTime();
  displayTime(throttleSecs, 8, 102, bottom_bg_color);

  //dispPowerCycles(104,100,2);
}

// track flight timer
void EppgDisplay::handleFlightTime() {
  if (!armed) {
    throttledFlag = true;
    throttled = false;
  } else { // armed
    // start the timer when armed and throttle is above the threshold
    if (throttle.getPWM() > 1250 && throttledFlag) {
      throttledAtMillis = millis();
      throttledFlag = false;
      throttled = true;
    }
    if (throttled) {
      throttleSecs = (millis()-throttledAtMillis) / 1000.0;
    } else {
      throttleSecs = 0;
    }
  }
}

// display altitude data on screen
void EppgDisplay::displayAlt() {
  float altiudeM = 0;
  // TODO make MSL explicit?
  if (armAltM > 0 && deviceData.sea_pressure != DEFAULT_SEA_PRESSURE) {  // MSL
    altiudeM = getAltitudeM();
  } else {  // AGL
    altiudeM = getAltitudeM() - armAltM;
  }

  // convert to ft if not using metric
  float alt = deviceData.metric_alt ? altiudeM : (round(altiudeM * 3.28084));

  dispValue(alt, lastAltM, 5, 0, 70, 102, 2, BLACK, bottom_bg_color);

  display.print(deviceData.metric_alt ? F("m") : F("ft"));
  lastAltM = alt;
}

// display first page (voltage and current)
void EppgDisplay::displayPage0() {
  float avgVoltage = getBatteryVoltSmoothed();

  dispValue(avgVoltage, prevVolts, 5, 1, 84, 42, 2, BLACK, DEFAULT_BG_COLOR);
  display.print("V");

  dispValue(telemetryData.amps, prevAmps, 3, 0, 108, 71, 2, BLACK, DEFAULT_BG_COLOR);
  display.print("A");

  float kWatts = getWatts() / 1000.0;
  kWatts = constrain(kWatts, 0, 50);

  dispValue(kWatts, prevKilowatts, 4, 1, 10, 42, 2, BLACK, DEFAULT_BG_COLOR);
  display.print("kW");

  float kwh = getWattHoursUsed() / 1000;
  dispValue(kwh, prevKwh, 4, 1, 10, 71, 2, BLACK, DEFAULT_BG_COLOR);
  display.print("kWh");

  display.setCursor(30, 60);
  display.setTextSize(1);
  if (deviceData.performance_mode == 0) {
    display.setTextColor(BLUE);
    display.print("CHILL");
  } else {
    display.setTextColor(RED);
    display.print("SPORT");
  }
}

// display second page (mAh and armed time)
void EppgDisplay::displayPage1() {
  dispValue(telemetryData.volts, prevVolts, 5, 1, 84, 42, 2, BLACK, DEFAULT_BG_COLOR);
  display.print("V");

  dispValue(telemetryData.amps, prevAmps, 3, 0, 108, 71, 2, BLACK, DEFAULT_BG_COLOR);
  display.print("A");

  float kwh = getWattHoursUsed() / 1000;
  dispValue(kwh, prevKilowatts, 4, 1, 10, 71, 2, BLACK, DEFAULT_BG_COLOR);
  display.print("kWh");

  display.setCursor(30, 60);
  display.setTextSize(1);
  if (deviceData.performance_mode == 0) {
    display.setTextColor(BLUE);
    display.print("CHILL");
  } else {
    display.setTextColor(RED);
    display.print("SPORT");
  }
}

void EppgDisplay::displayBootLoader() {
  display.fillScreen(DEFAULT_BG_COLOR);
  displayMessage("BL - UF2");
}

void EppgDisplay::displayDisarm() {
  bottom_bg_color = DEFAULT_BG_COLOR;
  display.fillRect(0, 93, 160, 40, bottom_bg_color);
  update();
}

void EppgDisplay::displayArm() {
  bottom_bg_color = ARMED_BG_COLOR;
  display.fillRect(0, 93, 160, 40, bottom_bg_color);
}

void EppgDisplay::displaySetCruise() {
    // update display to show cruise
    display.setCursor(70, 60);
    display.setTextSize(1);
    display.setTextColor(RED);
    display.print(F("CRUISE"));

    bottom_bg_color = YELLOW;
    display.fillRect(0, 93, 160, 40, bottom_bg_color);
}

void EppgDisplay::displayRemoveCruise() {
  // update bottom bar
  bottom_bg_color = DEFAULT_BG_COLOR;
  if (armed) { bottom_bg_color = ARMED_BG_COLOR; }
  display.fillRect(0, 93, 160, 40, bottom_bg_color);

  // update text status
  display.setCursor(70, 60);
  display.setTextSize(1);
  display.setTextColor(DEFAULT_BG_COLOR);
  display.print(F("CRUISE"));  // overwrite in bg color to remove
  display.setTextColor(BLACK);
}

/**
 * advance to next screen page
 *
 * @return the number of next page
 */
int EppgDisplay::nextPage() {
  display.fillRect(0, 37, 160, 54, DEFAULT_BG_COLOR);

  if (page >= LAST_PAGE) {
    return page = 0;
  }
  return ++page;
}

#endif // DISPLAY_DISABLED