#if defined(EPPG_BLE_HUB)

#include <Arduino.h>
#include "../../inc/eppgConfig.h"
#include "../../inc/esp32/globals.h"
#include "ble-hub.h"
#include "eppgPower.h"
#include "eppgESC.h"
#include "eppgSensors.h"
#include "eppgBMS.h"

#ifdef BLE_LATENCY_TEST
latency_test_t ble_lat_test;
#endif

extern EppgBLEServer ble;
extern EppgEsc esc;
extern STR_BMS_DATA bmsData;

void bleConnected(){Serial.println("Client Connected");}
void bleDisconnected() {Serial.println("Client Disconnected");}
void bleThrottleUpdate(int val) {
#ifdef BLE_TEST
  Serial.printf("Updated Throttle: %d\n", val);
#else
  esc.writeMicroseconds(val);
#endif
}

void bleArm() {
#ifdef BLE_TEST
  Serial.println("Armed from BLE");
#else
  esc.writeMicroseconds(ESC_DISARMED_PWM);
#endif
}

void bleDisarm() {
#ifdef BLE_TEST
  Serial.println("Disarmed from BLE");
#else
  esc.writeMicroseconds(ESC_DISARMED_PWM);
#endif
}

void setupBleServer() {
  SerialESC.begin(ESC_BAUD_RATE);
  SerialESC.setTimeout(ESC_TIMEOUT);

  esc.attach(ESC_PIN);
  esc.writeMicroseconds(ESC_DISARMED_PWM);

  ble.setConnectCallback(bleConnected);
  ble.setDisconnectCallback(bleDisconnected);
  ble.setThrottleCallback(bleThrottleUpdate);
  ble.setArmCallback(bleArm);
  ble.setDisarmCallback(bleDisarm);
  ble.begin();
}

void bleServerLoop() {
#if defined(BLE_TEST)
  ble.setBattery(random(0x00,0x64));
  #ifdef BLE_LATENCY_TEST
  ble_lat_test.count++;
  ble_lat_test.time = millis();
  ble.setStatus(ble_lat_test);
  #else
  ble.setStatus(random(0x00,0xFFFF));
  #endif
  double temp = random(1, 50);
  temp += (random(1, 99) / 100.0F);
  ble.setTemp(temp);
  double pressure = random(300, 0xFFFF);
  pressure += (random(1, 99) / 100.0F);
  ble.setBmp(pressure);
  
  // bmsData.packVoltage = 12.3;
  // bmsData.packCurrent = 4.5;
  // bmsData.packSOC = 0.75;
  // bmsData.maxCellmV = 4200;
  // bmsData.maxCellVNum = 3;
  // bmsData.minCellmV = 3800;
  // bmsData.minCellVNum = 7;
  // bmsData.cellDiff = 0.2;
  // bmsData.tempMax = 80;
  // bmsData.tempMin = 1;
  // bmsData.tempAverage = 25.5;
  // bmsData.chargeFetState = true;
  // bmsData.resCapacitymAh = 2000;
  // bmsData.numberOfCells = 20;
  // bmsData.numOfTempSensors = 4;
  // bmsData.chargeState = true;
  // bmsData.loadState = false;
  // bmsData.bmsCycles = 1000;
  ble.setBmsData(bmsData);
#else
  //ble.setBmp(getPressure()); // Get pressure first to set the temp value.
  //ble.setTemp(getAmbientTempC());
#endif
  delay(500);
}

#endif // EPPG_BLE_HUB
