#ifndef _EPPG_BLE_H
#define _EPPG_BLE_H

typedef void (*connectCallback) ();
typedef void (*disconnectCallback) ();
typedef void (*throttleCallback) (int val);
typedef void (*armCallback) ();
typedef void (*disarmCallback) ();

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
};

class EppgBLEClient {
  connectCallback    connectCB;
  disconnectCallback disconnectCB;
  bool               connecting;
public:
  void begin();
  void processEvent(unsigned long event);
  void setConnectCallback(connectCallback cb) { connectCB = cb; }
  void setDisconnectCallback(disconnectCallback cb) { disconnectCB = cb; }
  bool isConnecting() { return connecting; }
  bool isConnected();
  bool connect();
  bool setThrottle(int val);
};

#endif // _EPPG_BLE_H