#include <Arduino.h>
#include "eppgDisplay.h"
#include "eppgSensors.h"

#ifndef DISPLAY_DISABLED

#include "../../inc/eppgConfig.h"
#include <TimeLib.h>  // convert time to hours mins etc
#include "../../inc/esp32/structs.h"
#include "../../inc/esp32/globals.h"
#include "eppgPower.h"
#include "eppgThrottle.h"

#define LAST_PAGE 1  // starts at 0
#define BLACK                 TFT_BLACK
#define WHITE                 TFT_WHITE
#define GREEN                 TFT_GREEN
#define YELLOW                TFT_YELLOW
#define RED                   TFT_RED
#define BLUE                  TFT_BLUE
#define ORANGE                TFT_ORANGE
#define PURPLE                0x780F

#define DEFAULT_BG_COLOR      WHITE
#define ARMED_BG_COLOR        TFT_CYAN
#define CRUISE_BG_COLOR       YELLOW

#define DIGIT_ARRAY_SIZE      7

extern bool armed;
extern bool hub_armed;
extern STR_DEVICE_DATA_140_V1 deviceData;
extern STR_BMS_DATA bmsData;
extern EppgThrottle throttle;

TFT_eSPI tft = TFT_eSPI();

void updateDisplayTask(void * param) {
  EppgDisplay *display = (EppgDisplay*)param;
  for (;;) {
    display->update();
    delay(250); // Update display every 250ms
  }

  vTaskDelete(NULL); // should never reach this
}

EppgDisplay::EppgDisplay()
:page(0),
 bottom_bg_color(DEFAULT_BG_COLOR),
 screen_wiped(false),
 batteryFlag(true),
 throttledFlag(true),
 throttled(false),
 throttledAtMillis(0),
 throttleSecs(0),
 minutes(0),
 seconds(0),
 armAltM(0) {
}

uint8_t hh = 1, mm = 2, ss = 3; // Get H, M, S from compile time
uint32_t targetTime = 0;                    // for next 1 second timeout
byte omm = 99, oss = 99;
byte xcolon = 0, xsecs = 0;

unsigned int colour = 0;
SemaphoreHandle_t xSemaphore4tft;

void EppgDisplay::init() {
  xSemaphore4tft = xSemaphoreCreateBinary();
  xSemaphoreGive( xSemaphore4tft );

  if( xSemaphoreTake( xSemaphore4tft, ( TickType_t ) 100 ) == pdTRUE ) {
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);

    Serial.println("update disp");
    targetTime = millis() + 1000;
    displayMeta();
    xSemaphoreGive( xSemaphore4tft );
  }

  //pinMode(TFT_LITE, OUTPUT);
  //reset();
  //digitalWrite(TFT_LITE, HIGH);  // Backlight on
  delay(2500);
  if( xSemaphoreTake( xSemaphore4tft, ( TickType_t ) 100 ) == pdTRUE ) {
    tft.fillScreen(TFT_BLACK);
    xSemaphoreGive( xSemaphore4tft );
  }
  // could call xTaskCreatePinnedToCore to create a task that is pinned to a specific core
  xTaskCreate(updateDisplayTask, "updateDisplay", 20000, this, 1, NULL);
}

void EppgDisplay::reset() {
  tft.fillScreen(DEFAULT_BG_COLOR);
  tft.setTextColor(BLACK);
  tft.setCursor(0, 0);
  tft.setTextSize(1);
  tft.setTextWrap(true);

  tft.setRotation(deviceData.screen_rotation);  // 1=right hand, 3=left hand
}

void EppgDisplay::displayMeta() {
  tft.setFreeFont(&FreeSansBold24pt7b);
  tft.setCursor(50, 75);
  tft.setTextColor(TFT_WHITE);
  tft.println("OpenPPG");
  tft.setTextFont(4);
  tft.setCursor(120, 120);
  tft.print("v" + String(VERSION_MAJOR) + "." + String(VERSION_MINOR));
#ifdef RP_PIO
  tft.print("R");
#endif
  tft.setCursor(120, 150);
  displayTime(deviceData.armed_time);
}

void EppgDisplay::displayTime(int val) {
  int minutes = val / 60;  // numberOfMinutes(val);
  int seconds = numberOfSeconds(val);

  tft.print(convertToDigits(minutes));
  tft.print(":");
  tft.print(convertToDigits(seconds));
}

// TODO (bug) rolls over at 99mins
void EppgDisplay::displayTime(int val, int x, int y, uint16_t bg_color) {
  // display number of minutes and seconds (since armed and throttled)
  tft.setCursor(x, y);
  tft.setTextSize(2);
  tft.setTextColor(BLACK);
  minutes = val / 60;
  seconds = numberOfSeconds(val);
  if (minutes < 10) {
    tft.setCursor(x, y);
    tft.print("0");
  }
  dispValue(minutes, minutes, 2, 0, x, y, 2, BLACK, bg_color);
  tft.setCursor(x+24, y);
  tft.print(":");
  tft.setCursor(x+36, y);
  if (seconds < 10) {
    tft.print("0");
  }
  dispValue(seconds, seconds, 2, 0, x+36, y, 2, BLACK, bg_color);
}

// display hidden page (firmware version and total armed time)
void EppgDisplay::displayVersions() {
  tft.setTextSize(2);
  tft.print(F("v"));
  tft.print(VERSION_MAJOR);
  tft.print(F("."));
  tft.println(VERSION_MINOR);
  addVSpace();
  tft.setTextSize(2);
  displayTime(deviceData.armed_time);
  tft.print(F(" h:m"));
  // addVSpace();
  // tft.print(chipId()); // TODO: trim down
}

// display hidden page (firmware version and total armed time)
void EppgDisplay::displayMessage(const char *message) {
  tft.setCursor(0, 0);
  tft.setTextSize(2);
  tft.println(message);
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
  tft.setTextSize(1);
  tft.println(" ");
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

  tft.setTextSize(textSize);
  tft.setCursor(x, y);

  // PRINT LEADING SPACES TO RIGHT-ALIGN:
  tft.setTextColor(background);
  for(int i=0; i<(maxDigits-numDigits); i++){
    tft.print(static_cast<char>(218));
  }
  tft.setTextColor(textColor);

  // ERASE ONLY THE NECESSARY DIGITS:
  for (int i=0; i<numDigits; i++) {
    if (digit[i]!=prevDigit[i]) {
      tft.setTextColor(background);
      tft.print(static_cast<char>(218));
    } else {
      tft.setTextColor(textColor);
      tft.print(digit[i]);
    }
  }

  // BACK TO THE BEGINNING:
  tft.setCursor(x, y);

  // ADVANCE THE CURSOR TO THE PROPER LOCATION:
  tft.setTextColor(background);
  for (int i=0; i<(maxDigits-numDigits); i++) {
    tft.print(static_cast<char>(218));
  }
  tft.setTextColor(textColor);

  // PRINT THE DIGITS THAT NEED UPDATING
  for(int i=0; i<numDigits; i++){
    tft.print(digit[i]);
  }
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
  if( xSemaphoreTake( xSemaphore4tft, ( TickType_t ) 100 ) == pdTRUE ) {
    // update the clock every second
    displayClock();
    displayDiagnostics();
    xSemaphoreGive( xSemaphore4tft );
  }
}

void EppgDisplay::displayDiagnostics(){
  tft.setCursor(0, 35, 4);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.println("Diagnostics Mode");
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print("Voltage: ");
  tft.println(bmsData.packVoltage);
}

void EppgDisplay::displayClock() {
 if (targetTime < millis()) {
    // Set next update for 1 second later
    targetTime = millis() + 1000;
    tft.drawFastHLine(0, 25, tft.width(), TFT_WHITE);

    // Adjust the time values by adding 1 second
    ss++;              // Advance second
    if (ss == 60) {    // Check for roll-over
      ss = 0;          // Reset seconds to zero
      omm = mm;        // Save last minute time for display update
      mm++;            // Advance minute
      if (mm > 59) {   // Check for roll-over
        mm = 0;
        hh++;          // Advance hour
        if (hh > 23) { // Check for 24hr roll-over (could roll-over on 13)
          hh = 0;      // 0 for 24 hour clock, set to 1 for 12 hour clock
        }

      }
    }

    // Update digital time
    int xpos = 120;
    int ypos = 4; // Top left corner ot clock text, about half way down
    int ysecs = ypos + 0;

    if (omm != mm) { // Redraw hours and minutes time every minute
      omm = mm;
      // Draw hours and minutes
      if (hh < 10) xpos += tft.drawChar('0', xpos, ypos, 2); // Add hours leading zero for 24 hr clock
      xpos += tft.drawNumber(hh, xpos, ypos, 2);             // Draw hours
      xcolon = xpos; // Save colon coord for later to flash on/off later
      xpos += tft.drawChar(':', xpos, ypos - 2, 2);
      if (mm < 10) xpos += tft.drawChar('0', xpos, ypos, 2); // Add minutes leading zero
      xpos += tft.drawNumber(mm, xpos, ypos, 2);             // Draw minutes
      xsecs = xpos; // Sae seconds 'x' position for later display updates
    }
    if (oss != ss) { // Redraw seconds time every second
      oss = ss;
      xpos = xsecs;

      if (ss % 2) { // Flash the colons on/off
        tft.setTextColor(0x39C4, TFT_BLACK);        // Set colour to grey to dim colon
        tft.drawChar(':', xcolon, ypos - 2, 2);     // Hour:minute colon
        xpos += tft.drawChar(':', xsecs, ysecs, 2); // Seconds colon
        tft.setTextColor(TFT_WHITE, TFT_BLACK);    // Set colour back to yellow
      }
      else {
        tft.drawChar(':', xcolon, ypos - 2, 2);     // Hour:minute colon
        xpos += tft.drawChar(':', xsecs, ysecs, 2); // Seconds colon
      }

      //Draw seconds
      if (ss < 10) xpos += tft.drawChar('0', xpos, ysecs, 2); // Add leading zero
      tft.drawNumber(ss, xpos, ysecs, 2);                     // Draw seconds
    }
  }
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
  float altitudeM = 0;
  // TODO make MSL explicit?
  if (armAltM > 0 && deviceData.sea_pressure != DEFAULT_SEA_PRESSURE) {  // MSL
    altitudeM = getAltitudeM();
  } else {  // AGL
    altitudeM = getAltitudeM() - armAltM;
  }

  // convert to ft if not using metric
  float alt = deviceData.metric_alt ? altitudeM : (round(altitudeM * 3.28084));

  dispValue(alt, alt, 5, 0, 70, 102, 2, BLACK, bottom_bg_color);

  tft.print(deviceData.metric_alt ? F("m") : F("ft"));
}

// display first page (voltage and current)
void EppgDisplay::displayPage0() {
  float avgVoltage = getBatteryVoltSmoothed();
  STR_ESC_TELEMETRY_140 telemetryData = getTelemetryData();

  dispValue(avgVoltage, avgVoltage, 5, 1, 84, 42, 2, BLACK, DEFAULT_BG_COLOR);
  tft.print("V");

  dispValue(telemetryData.amps, telemetryData.amps, 3, 0, 108, 71, 2, BLACK, DEFAULT_BG_COLOR);
  tft.print("A");

  float kWatts = getWatts() / 1000.0;
  kWatts = constrain(kWatts, 0, 50);

  dispValue(kWatts, kWatts, 4, 1, 10, 42, 2, BLACK, DEFAULT_BG_COLOR);
  tft.print("kW");

  float kwh = getWattHoursUsed() / 1000;
  dispValue(kwh, kwh, 4, 1, 10, 71, 2, BLACK, DEFAULT_BG_COLOR);
  tft.print("kWh");

  tft.setCursor(30, 60);
  tft.setTextSize(1);
  if (deviceData.performance_mode == 0) {
    tft.setTextColor(BLUE);
    tft.print("CHILL");
  } else {
    tft.setTextColor(RED);
    tft.print("SPORT");
  }
}

// display second page (mAh and armed time)
void EppgDisplay::displayPage1() {
  STR_ESC_TELEMETRY_140 telemetryData = getTelemetryData();

  dispValue(telemetryData.volts, telemetryData.volts, 5, 1, 84, 42, 2, BLACK, DEFAULT_BG_COLOR);
  tft.print("V");

  dispValue(telemetryData.amps, telemetryData.amps, 3, 0, 108, 71, 2, BLACK, DEFAULT_BG_COLOR);
  tft.print("A");

  float kwh = getWattHoursUsed() / 1000;
  dispValue(kwh, kwh, 4, 1, 10, 71, 2, BLACK, DEFAULT_BG_COLOR);
  tft.print("kWh");

  tft.setCursor(30, 60);
  tft.setTextSize(1);
  if (deviceData.performance_mode == 0) {
    tft.setTextColor(BLUE);
    tft.print("CHILL");
  } else {
    tft.setTextColor(RED);
    tft.print("SPORT");
  }
}

void EppgDisplay::displayBootLoader() {
  tft.fillScreen(DEFAULT_BG_COLOR);
  displayMessage("BL - UF2");
}

void EppgDisplay::displayDisarm() {
  bottom_bg_color = DEFAULT_BG_COLOR;
  tft.fillRect(0, 93, 160, 40, bottom_bg_color);
  update();
}

void EppgDisplay::displayArm() {
  this->armAltM = getAltitudeM();
  bottom_bg_color = ARMED_BG_COLOR;
  tft.fillRect(0, 93, 160, 40, bottom_bg_color);
}

void EppgDisplay::displaySetCruise() {
    // update display to show cruise
    tft.setCursor(70, 60);
    tft.setTextSize(1);
    tft.setTextColor(RED);
    tft.print(F("CRUISE"));

    bottom_bg_color = YELLOW;
    tft.fillRect(0, 93, 160, 40, bottom_bg_color);
}

void EppgDisplay::displayRemoveCruise() {
  // update bottom bar
  bottom_bg_color = DEFAULT_BG_COLOR;
  if (armed) { bottom_bg_color = ARMED_BG_COLOR; }
  tft.fillRect(0, 93, 160, 40, bottom_bg_color);

  // update text status
  tft.setCursor(70, 60);
  tft.setTextSize(1);
  tft.setTextColor(DEFAULT_BG_COLOR);
  tft.print(F("CRUISE"));  // overwrite in bg color to remove
  tft.setTextColor(BLACK);
}

/**
 * advance to next screen page
 *
 * @return the number of next page
 */
int EppgDisplay::nextPage() {
  tft.fillRect(0, 37, 160, 54, DEFAULT_BG_COLOR);

  if (page >= LAST_PAGE) {
    return page = 0;
  }
  return ++page;
}

#endif // DISPLAY_DISABLED
