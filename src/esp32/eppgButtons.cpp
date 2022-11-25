#ifndef EPPG_BLE_HUB

#include <eppgButtons.h>
#include "../../inc/eppgConfig.h"
#include <AceButton.h>           // button clicks
#include "../../inc/esp32/structs.h"
#include "../../inc/esp32/globals.h"

#include "eppgStorage.h"
#include "eppgBuzzer.h"
#include "eppgThrottle.h"

using namespace ace_button;

extern bool armed;
extern bool cruising;
extern STR_DEVICE_DATA_140_V1 deviceData;
extern EppgThrottle throttle;

static xTimerHandle checkButtonsHandle;
static AceButton button_top(BUTTON_TOP);
static ButtonConfig* buttonConfig = button_top.getButtonConfig();

void checkButtons(xTimerHandle pxTimer) {
  button_top.check();
}

// The event handler for the the buttons
void handleButtonEvent(AceButton* /* btn */, uint8_t eventType, uint8_t /* st */) {
  switch (eventType) {
  case AceButton::kEventDoubleClicked:
    if (armed) {
      disarmSystem();
    } else if (throttle.safe()) {
      armSystem();
    } else {
      handleArmFail();
    }
    break;
  case AceButton::kEventLongPressed:
    if (armed) {
      if (throttle.getCruising()) {
        removeCruise(true);
      } else {
        setCruise();
      }
    } else {
      // show stats screen?
    }
    break;
  case AceButton::kEventLongReleased:
    break;
  }
}

// on boot check for button to switch mode
void modeSwitch() {
  if (!button_top.isPressedRaw()) { return; }

  // 0=CHILL 1=SPORT 2=LUDICROUS?!
  if (deviceData.performance_mode == 0) {
    deviceData.performance_mode = 1;
  } else {
    deviceData.performance_mode = 0;
  }
  writeDeviceData();
  uint16_t notify_melody[] = { 900, 1976 };
  playMelody(notify_melody, 2);
}

// inital button setup and config
void initButtons() {
  pinMode(BUTTON_TOP, INPUT_PULLUP);

  buttonConfig->setEventHandler(handleButtonEvent);
  buttonConfig->setFeature(ButtonConfig::kFeatureDoubleClick);
  buttonConfig->setFeature(ButtonConfig::kFeatureLongPress);
  buttonConfig->setFeature(ButtonConfig::kFeatureSuppressAfterDoubleClick);
  buttonConfig->setFeature(ButtonConfig::kFeatureSuppressAfterLongPress);
  buttonConfig->setLongPressDelay(2500);
  buttonConfig->setDoubleClickDelay(600);

  // 5ms timer to check buttons
  checkButtonsHandle = xTimerCreate("checkButtons", pdMS_TO_TICKS(5), pdTRUE, NULL, checkButtons);
  xTimerReset(checkButtonsHandle, portMAX_DELAY);
}

#endif // EPPG_BLE_HUB
