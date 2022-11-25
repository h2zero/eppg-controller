#if defined(EPPG_BLE_HANDHELD)

#include <Arduino.h>
#include "../../inc/eppgConfig.h"
#include "../../inc/esp32/globals.h"
#include "ble-handheld.h"
#include <eppgBLE.h>

#ifdef BLE_LATENCY_TEST
latency_test_t ble_lat_test;
latency_test_t ble_lat_last;
uint32_t disconnect_count;
#endif

extern EppgBLEClient ble;
extern bool armed;
static STR_ESC_TELEMETRY_140 telemetryData;

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

void setupBleClient() {
  ble.setStatusCallback(bleStatusUpdate);
  ble.setBatteryCallback(bleBatteryUpdate);
  ble.begin();
}

void bleClientLoop() {
#if defined(BLE_TEST)
  if (!armed) {
    armSystem();
  } else {
    disarmSystem();
  }
  Serial.printf("Temp: %f, pressure %f\n", ble.getTemp(), ble.getBmp());
  delay(500);
#endif
}

const STR_ESC_TELEMETRY_140& getTelemetryData() {
  return telemetryData;
}

#endif // EPPG_BLE_HANDHELD
