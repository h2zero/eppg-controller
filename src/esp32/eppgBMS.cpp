#ifndef BMS_TELEM_DISABLED
#ifdef EPPG_BLE_HUB

#include <Arduino.h>
#include "eppgBMS.h"
#include <CircularBuffer.h>
#include "../../inc/esp32/globals.h"
#include "eppgPower.h"

extern STR_DEVICE_DATA_140_V1 deviceData;
static STR_ESC_TELEMETRY_140 telemetryData;

const STR_BMS_TELEMETRY_140& getBmsData() {
  return telemetryData;
}

void handleBmsTask(void * param) {
  EppgBms *bms = (EppgBms*)param;
  for (;;) {
    bms->handleBms();
    delay(250);
  }

  vTaskDelete(NULL); // should never reach this
}

EppgBms::EppgBms()
: bmsData{0},
  _volts(0),
  _temperatureC(0),
  _amps(0),
  _eRPM(0),
  _inPWM(0),
  _outPWM(0) {
}

void EppgBms::begin() {
  SerialBMS.begin(BMS_BAUD_RATE);
  SerialBMS.setTimeout(BMS_TIMEOUT);
  xTaskCreate(handleBmsTask, "handleBms", 5000, this, 1, NULL);
}

void EppgBms::handleBms() {
  Serial.println(F("Handling Bms"));
  bmsDriver.update();
  telemetryData.volts = bmsDriver.get.packVoltage;
}

// for debugging
void EppgBms::printDebug() {
  Serial.println("basic BMS Data:   " + (String)bmsDriver.get.packVoltage + "V " + (String)bmsDriver.get.packCurrent + "I " + (String)bmsDriver.get.packSOC + "\% ");
  Serial.println("Package Temperature:  " + (String)bmsDriver.get.tempAverage);
  Serial.println("BMS Heartbeat:  " + (String)bmsDriver.get.bmsHeartBeat); // cycle 0-255
}

#endif // EPPG_BLE_HUB
#endif // BMS_TELEM_DISABLED