# EPPG Controller BLE API

The following describes the API functions for the BLE wireless communication between the handheld controller and the hub.

## Classes
- **EppgBLEClient:** This is used by the handheld device to send commands and receive data from the hub.

- **EppgBLEServer:** This is used by the hub device and contains the data read by the handheld device and performs all sensor readings and ESC controls.

### EppgBLEServer (Hub) API
To use: create an instance of the class, i.e: `EppgBLEServer ble`

#### Member Functions
```
void begin();
```
Starts the BLE stack and configures the services and characteristics, should be called only after the callbacks have been set.

```
void setConnectCallback(connectCallback cb)
void setDisconnectCallback(disconnectCallback cb)
void setThrottleCallback(throttleCallback cb)
void setArmCallback(armCallback cb)
void setDisarmCallback(disarmCallback cb)
```
These set callback functions that will be called when the events named occur. None are mandatory to set and can be NULL if not used.
The function declarations are as follows:
```
void connectCallback();         // Called when the handheld has connected
void disconnectCallback();      // Called when handheld disconnects
void throttleCallback(int val); // Called when new throttle value is sent by the handheld
void armCallback();             // Called when the arm command is sent by the handheld
void disarmCallback()           // Called when the disarm command is sent by the handheld
```

The following functions set the data that is read by the Handheld (client) device:

```
void setStatus(uint32_t val);
```
(WIP) Set the status flags.

```
void setBattery(uint8_t val);
```
Set the battery level (0-100%).

```
void setTemp(double temp);
```
Set the temperature from the BMP sensor.

```
void setBmp(double pressure);
```
Set the barometric pressure from the BMP sensor.

### EppgBLEClient (Handheld) API
To use: create an instance of the class, i.e: `EppgBLEClient ble`

#### Member Functions
```
void begin();
```
Starts the BLE stack and begins scanning for a hub to connect with. Should be called only after callbacks have been set.
```
void setConnectCallback(connectCallback cb)
void setDisconnectCallback(disconnectCallback cb)
void setStatusCallback(statusCallback cb)
void setBatteryCallback(batteryCallback cb)
```
These set callback functions that will be called when the events named occur. None are mandatory to set and can be NULL if not used.

The function declarations are as follows:
```
void connectCallback();            // Called when connected to the hub
void disconnectCallback();         // Called when disconnected from the hub
void statusCallback(uint32_t val); // Called when a status notification is received
void batteryCallback(uint8_t val); // Called when a battery level notification is received
```

The following are command functions to send to the Hub (server) device:

```
bool setThrottle(int val);
```
Set a new throttle setting on the Hub. Return: `true` = success.

```
bool arm();
```
Arm the hub ESC. Return: `true` = success.

```
bool disarm();
```
Disarm the hub ESC. Return: `true` = success.

```
float getBmp();
```
Get the barometric pressure from the Hub. Return: `float` = value in pascal units of 0.1pa.

```
float getTemp();
```
Get the temperature from the Hub. Return: `float` = value in degrees Celsius.

```
bool connect();
```
Force a connection attempt (used internally, not intended for application use). Return: `true` = success

```
bool disconnect();
```
Force a disconnection from the Hub. Return: `true` = success.

```
bool isConnecting();
```
Returns `true` if a connection attempt is in progress.

```
bool isConnected();
```
Returns `true` if connected to a Hub.

## General information
The classes have been designed to be automatic in nature. When the respective `begin` functions have been called all connection management/scanning/advertising are handled as necessary. This means when the devices disconnect the Hub (server) will automatically start advertising and the Handheld device (client) will start scanning in order to reconnect as soon as possible.

If a disconnection event occurs there is no need to handle anything by the application, all calls to the API can continue without worry of error, though commands while disconnected will not be received.