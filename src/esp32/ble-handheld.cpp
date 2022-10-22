#if defined(EPPG_BLE_CLIENT)

#include <Arduino.h>
#include "../../inc/esp32/esp32-config.h"
#include "../../inc/esp32/globals.h"
#include "ble-handheld.h"
#include <eppgBLE.h>
#include "eppgDisplay.h"
#include "eppgButtons.h"
#include "eppgThrottle.h"

#ifdef BLE_LATENCY_TEST
latency_test_t ble_lat_test;
latency_test_t ble_lat_last;
uint32_t disconnect_count;
#endif

static xTimerHandle ledBlinkHandle;
static xTimerHandle checkButtonsHandle;

extern EppgBLEClient ble;
extern EppgDisplay display;
extern EppgThrottle throttle;
extern bool armed;


void bleConnected(){Serial.println("Connected to server");}
void bleDisconnected() {
  Serial.println("Disconnected from server");
  #ifdef BLE_LATENCY_TEST
  ble_lat_test = {0};
  ble_lat_last = {0};
  disconnect_count++;
  #endif
}

#ifdef BLE_LATENCY_TEST
void bleStatusUpdate(latency_test_t &lat, int rssi) {
  unsigned long cur_millis = millis();
  unsigned long local_lat = cur_millis - ble_lat_test.time;
  unsigned long remote_lat = lat.time - ble_lat_last.time;
  unsigned long latency = (local_lat < remote_lat) ? 0 : local_lat - remote_lat;
  if (ble_lat_test.count) {
  //  Serial.printf("cur: %u, last: %u, rem_lat: %u, last_rem_lat: %u\n",
  //                cur_millis, ble_lat_test.time, lat.time, ble_lat_last.time);
    Serial.printf("Packet loss count: %u, Latency: %u, disconnects: %u, rssi: %d\n",
                  lat.count - (++ble_lat_test.count), latency, disconnect_count, rssi);
    ble_lat_test.time = cur_millis;
  } else {
    ble_lat_test.count = lat.count;
    ble_lat_test.time = cur_millis;
  }
  ble_lat_last = lat;
}
#else
void bleStatusUpdate(uint32_t val){Serial.printf("Status update: %08x\n", val);}
#endif // BLE_LATENCY_TEST

void bleBatteryUpdate(uint8_t val){Serial.printf("Battery update: %u\n", val);}

void updateDisplayTask(void * parameter) {
  for (;;) {
    display.update();
    delay(250); // Update display every 250ms
  }

  vTaskDelete(NULL); // should never reach this
}

void handleThrottleTask(void * parameter) {
  for (;;) {
    throttle.handleThrottle();
#ifdef BLE_TEST
    delay(100);
#else
    delay(22); // Update throttle every 22ms
#endif
  }

  vTaskDelete(NULL); // should never reach this
}

void stopBlinkTimer() {
  xTimerStop(ledBlinkHandle, portMAX_DELAY);
}

void startBlinkTimer() {
  xTimerReset(ledBlinkHandle, portMAX_DELAY);
}

void setupBleClient() {
  ble.setStatusCallback(bleStatusUpdate);
  ble.setBatteryCallback(bleBatteryUpdate);
  ble.begin();

  // 500ms timer to blink the LED.
  ledBlinkHandle = xTimerCreate("blinkLED", pdMS_TO_TICKS(500), pdTRUE, NULL, blinkLED);
  xTimerReset(ledBlinkHandle, portMAX_DELAY);

  // 5ms timer to check buttons
  checkButtonsHandle = xTimerCreate("checkButtons", pdMS_TO_TICKS(5), pdTRUE, NULL, checkButtons);
  xTimerReset(checkButtonsHandle, portMAX_DELAY);

  xTaskCreate(updateDisplayTask, "updateDisplay", 5000, NULL, 1, NULL);
  xTaskCreate(handleThrottleTask, "handleThrottle", 5000, NULL, 2, NULL);
}

void bleClientLoop() {
#if defined(BLE_TEST)
  if (!armed) {
    armSystem();
  } else {
    disarmSystem();
  }
  Serial.printf("Temp: %f, pressure %f\n", ble.getTemp(), ble.getBmp());
  delay(1000);
#endif
}

#endif // EPPG_BLE_CLIENT
