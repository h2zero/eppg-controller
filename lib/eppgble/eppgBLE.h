#ifndef _EPPG_BLE_H
#define _EPPG_BLE_H

#include <NimBLEDevice.h>

/* Generated UUID's */
const char* const MAIN_SERVICE_UUID = "8877FB19-E00B-40BE-905E-ACB42C39E6B8";
const char* const TH_CHAR_UUID = "5A57F691-C0B9-45DD-BDF1-279681212C29";
const char* const STATUS_CHAR_UUID = "28913A56-5701-4B27-85DB-50985F224847";
const char* const ARM_CHAR_UUID = "aeb83642-7cc4-45e9-bdf9-c9450876d98d";

/* BLE standard UUID's */
const char* const BATT_SERVICE_UUID = "180F";
const char* const BATT_CHAR_UUID = "2A19";
const char* const ENV_SERVICE_UUID = "181A";
const char* const TEMP_CHAR_UUID = "2A6E";
const char* const BARO_CHAR_UUID = "2A6D"; // TODO: Change to to 2AB3 (elevation)

typedef struct {
  uint32_t count;
  unsigned long time;
} latency_test_t;

typedef void (*connectCallback) ();
typedef void (*disconnectCallback) ();
typedef void (*throttleCallback) (int val);
typedef void (*armCallback) ();
typedef void (*disarmCallback) ();
#ifdef BLE_LATENCY_TEST
typedef void (*statusCallback) (latency_test_t &lat, int rssi);
#else
typedef void (*statusCallback) (uint32_t val);
#endif
typedef void (*batteryCallback) (uint8_t val);

enum bleEvent {
  CONNECTED,
  DISCONNECTED,
  THROTTLE_UPDATE,
  ARM,
  DISARM,
  DEVICE_FOUND,
};

class EppgBLEServer {
  connectCallback    connectCB;
  disconnectCallback disconnectCB;
  throttleCallback   throttleCB;
  armCallback        armCB;
  disarmCallback     disarmCB;
public:
  void begin();
  void processEvent(unsigned long event);
  void setConnectCallback(connectCallback cb) { connectCB = cb; }
  void setDisconnectCallback(disconnectCallback cb) { disconnectCB = cb; }
  void setThrottleCallback(throttleCallback cb) { throttleCB = cb; }
  void setArmCallback(armCallback cb) { armCB = cb; }
  void setDisarmCallback(disarmCallback cb) { disarmCB = cb; }
#ifdef BLE_LATENCY_TEST
  void setStatus(latency_test_t &lat);
#else
  void setStatus(uint32_t val);
#endif
  void setBattery(uint8_t val);
  void setTemp(double temp);
  void setBmp(double pressure);
};

class EppgBLEClient {
  void statusNotify(NimBLERemoteCharacteristic *pChar,
                    uint8_t *pData, size_t length, bool isNotify);
  void batteryNotify(NimBLERemoteCharacteristic *pChar,
                     uint8_t *pData, size_t length, bool isNotify);
  connectCallback    connectCB;
  disconnectCallback disconnectCB;
  statusCallback     statusCB;
  batteryCallback    batteryCB;
  bool               connecting;
public:
  void begin();
  void processEvent(unsigned long event);
  void setConnectCallback(connectCallback cb) { connectCB = cb; }
  void setDisconnectCallback(disconnectCallback cb) { disconnectCB = cb; }
  void setStatusCallback(statusCallback cb) { statusCB = cb; }
  void setBatteryCallback(batteryCallback cb) { batteryCB = cb; }
  bool isConnecting() { return connecting; }
  bool isConnected();
  bool connect();
  bool disconnect();
  bool setThrottle(int val);
  bool arm();
  bool disarm();
  float getBmp();
  float getTemp();
};

#endif // _EPPG_BLE_H