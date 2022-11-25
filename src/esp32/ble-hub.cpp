#if defined(EPPG_BLE_HUB)

#include <Arduino.h>
#include "../../inc/esp32/esp32-config.h"
#include "../../inc/esp32/globals.h"
#include "ble-hub.h"
#include "eppgPower.h"
#include "eppgESC.h"
#include "eppgSensors.h"


#ifdef BLE_LATENCY_TEST
latency_test_t ble_lat_test;
#endif

extern EppgBLEServer ble;
extern EppgEsc esc;

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

void handleTelemetryTask(void * parameter) {
  for (;;) {
    esc.handleTelemetry();
#ifndef BLE_TEST
    ble.setBmp(getAltitudeM()); // Get pressure first to set the temp value.
    ble.setTemp(getAmbientTempC());
#else
    double temp = random(1, 50);
    temp += (random(1, 99) / 100.0F);
    ble.setTemp(temp);
    double pressure = random(300, 0xFFFF);
    pressure += (random(1, 99) / 100.0F);
    ble.setBmp(pressure);
#endif
    delay(50);
  }

  vTaskDelete(NULL); // should never reach this
}

void trackPowerTask(void * parameter) {
  for (;;) {
    trackPower();
    delay(250);
  }

  vTaskDelete(NULL); // should never reach this
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

  xTaskCreate(trackPowerTask, "trackPower", 5000, NULL, 1, NULL);
  xTaskCreate(handleTelemetryTask, "handleTelemetry", 5000, NULL, 1, NULL);
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
  delay(500);
#endif
}

#endif // EPPG_BLE_HUB
