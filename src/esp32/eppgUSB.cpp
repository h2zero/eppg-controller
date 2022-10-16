// Copyright 2020 <Zach Whitehead>
// OpenPPG

#ifdef USE_TINYUSB

#include <Arduino.h>
#include <ArduinoJson.h>
#ifdef M0_PIO
  #include "../../inc/sp140/m0-config.h"          // device config
#elif RP_PIO
  #include "../../inc/sp140/rp2040-config.h"         // device config
#elif ESP_PLATFORM
  #include "../../inc/esp32/esp32-config.h"
#endif
#include "../../inc/esp32/globals.h"
#include "../../inc/esp32/structs.h"
#include "eppgUSB.h"
#include "eppgDisplay.h"
#include "eppgUtils.h"
#include "eppgStorage.h"

#ifdef ESP_PLATFORM
  #if ARDUINO_USB_MODE
    #error USB OTG Mode must be enabled.
  #endif
  #include "USB.h"
  USBCDC usb_web;
#else
  #include "Adafruit_TinyUSB.h"
  Adafruit_USBD_WebUSB usb_web;
#endif

extern EppgDisplay display;
extern STR_DEVICE_DATA_140_V1 deviceData;

// ** Logic for WebUSB **

void send_usb_serial() {
#ifdef M0_PIO
  const size_t capacity = JSON_OBJECT_SIZE(11) + 90;
  DynamicJsonDocument doc(capacity);

  doc["major_v"] = VERSION_MAJOR;
  doc["minor_v"] = VERSION_MINOR;
  doc["arch"] = "SAMD21";
  doc["screen_rot"] = deviceData.screen_rotation;
  doc["armed_time"] = deviceData.armed_time;
  doc["metric_temp"] = deviceData.metric_temp;
  doc["metric_alt"] = deviceData.metric_alt;
  doc["performance_mode"] = deviceData.performance_mode;
  doc["sea_pressure"] = deviceData.sea_pressure;
  doc["device_id"] = chipId();

  char output[256];
  serializeJson(doc, output);
  usb_web.println(output);
#elif RP_PIO
  StaticJsonDocument<256> doc; // <- a little more than 256 bytes in the stack

  doc["mj_v"].set(VERSION_MAJOR);
  doc["mi_v"].set(VERSION_MINOR);
  doc["arch"].set("RP2040");
  doc["scr_rt"].set(deviceData.screen_rotation);
  doc["ar_tme"].set(deviceData.armed_time);
  doc["m_tmp"].set(deviceData.metric_temp);
  doc["m_alt"].set(deviceData.metric_alt);
  doc["prf"].set(deviceData.performance_mode);
  doc["sea_p"].set(deviceData.sea_pressure);
  //doc["id"].set(chipId()); // webusb bug prevents this extra field from being sent

  char output[256];
  serializeJson(doc, output, sizeof(output));
  usb_web.println(output);
  usb_web.flush();
  //Serial.println(chipId());
#endif // M0_PIO/RP_PIO
}

void line_state_callback(bool connected) {
  digitalWrite(LED_SW, connected);

  if ( connected ) send_usb_serial();
}

bool sanitizeDeviceData() {
  bool changed = false;

  if (deviceData.screen_rotation == 1 || deviceData.screen_rotation == 3) {
  } else {
    deviceData.screen_rotation = 3;
    changed = true;
  }
  if (deviceData.sea_pressure < 0 || deviceData.sea_pressure > 10000) {
    deviceData.sea_pressure = 1013.25;
    changed = true;
  }
  if (deviceData.metric_temp != true && deviceData.metric_temp != false) {
    deviceData.metric_temp = true;
    changed = true;
  }
  if (deviceData.metric_alt != true && deviceData.metric_alt != false) {
    deviceData.metric_alt = true;
    changed = true;
  }
  if (deviceData.performance_mode < 0 || deviceData.performance_mode > 1) {
    deviceData.performance_mode = 0;
    changed = true;
  }
  if (deviceData.batt_size < 0 || deviceData.batt_size > 10000) {
    deviceData.batt_size = 4000;
    changed = true;
  }
  return changed;
}

// customized for sp140
void parse_usb_serial() {
  if (!usb_web.available()) {
    return;
  }

  const size_t capacity = JSON_OBJECT_SIZE(12) + 90;
  DynamicJsonDocument doc(capacity);
  deserializeJson(doc, usb_web);

  if (doc["command"] && doc["command"] == "rbl") {
    display.displayBootLoader();
    rebootBootloader();
    return;  // run only the command
  }

  if (doc["major_v"] < 5) return;

  deviceData.screen_rotation = doc["screen_rot"].as<unsigned int>();  // "3/1"
  deviceData.sea_pressure = doc["sea_pressure"];  // 1013.25 mbar
  deviceData.metric_temp = doc["metric_temp"];  // true/false
  deviceData.metric_alt = doc["metric_alt"];  // true/false
  deviceData.performance_mode = doc["performance_mode"];  // 0,1
  deviceData.batt_size = doc["batt_size"];  // 4000
  sanitizeDeviceData();
  writeDeviceData();
  display.reset();
  send_usb_serial();
}

void initWebUSB() {
#ifdef ESP_PLATFORM
  USB.webUSB(true);
  USB.webUSBURL("https://config.openppg.com");
  USB.begin();
#else
  usb_web.begin();
  usb_web.setLandingPage(&landingPage);
  usb_web.setLineStateCallback(line_state_callback);
#endif
}

#endif // USE_TINYUSB