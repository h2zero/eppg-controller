#ifndef _EPPG_BLE_H
#define _EPPG_BLE_H

#include <NimBLEDevice.h>

typedef void (*connectCallback) ();
typedef void (*disconnectCallback) ();
typedef void (*throttleCallback) (int val);
typedef void (*armCallback) ();
typedef void (*disarmCallback) ();
typedef void (*statusCallback) (uint32_t val);
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
  void setStatus(uint32_t val);
  void setBattery(uint8_t val);
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
};

#endif // _EPPG_BLE_H