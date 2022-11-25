// Copyright 2019 <Zach Whitehead>
// OpenPPG
#include <Arduino.h>
#include "../../inc/eppgConfig.h"
#include "../../inc/esp32/structs.h"
#include "../../inc/esp32/globals.h"
#include "eppgUtils.h"

#ifdef M0_PIO

#define DBL_TAP_PTR ((volatile uint32_t *)(HMCRAMC0_ADDR + HMCRAMC0_SIZE - 4))
#define DBL_TAP_MAGIC 0xf01669ef  // Randomly selected, adjusted to have first and last bit set
#define DBL_TAP_MAGIC_QUICK_BOOT 0xf02669ef

#endif

extern STR_DEVICE_DATA_140_V1 deviceData;

// Map double values
double mapd(double x, double in_min, double in_max, double out_min, double out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// for debugging
void printDeviceData() {
  Serial.print("version major ");
  Serial.println(deviceData.version_major);
  Serial.print("version minor ");
  Serial.println(deviceData.version_minor);
  Serial.print("armed_time ");
  Serial.println(deviceData.armed_time);
  Serial.print("crc ");
  Serial.println(deviceData.crc);
}

#ifdef M0_PIO
// get chip serial number (for SAMD21)
String chipId() {
  volatile uint32_t val1, val2, val3, val4;
  volatile uint32_t *ptr1 = (volatile uint32_t *)0x0080A00C;
  val1 = *ptr1;
  volatile uint32_t *ptr = (volatile uint32_t *)0x0080A040;
  val2 = *ptr;
  ptr++;
  val3 = *ptr;
  ptr++;
  val4 = *ptr;

  char id_buf[33];
  sprintf(id_buf, "%8x%8x%8x%8x", val1, val2, val3, val4);
  return String(id_buf);
}
#elif RP_PIO
String chipId() {
  int len = 2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1;
  uint8_t buff[len] = "";
  pico_get_unique_board_id_string((char *)buff, len);
  return String((char *)buff);
}
#endif // M0_PIO/RP_PIO

#ifdef M0_PIO
// reboot/reset controller
void(* resetFunc) (void) = 0;  // declare reset function @ address 0

// sets the magic pointer to trigger a reboot to the bootloader for updating
void rebootBootloader() {
  *DBL_TAP_PTR = DBL_TAP_MAGIC;

  resetFunc();
}

#elif RP_PIO

// reboot/reset controller
void rebootBootloader() {
#ifdef USE_TINYUSB
  TinyUSB_Port_EnterDFU();
#endif
}
#elif ESP_PLATFORM
void rebootBootloader() {
  //TODO enter DFU mode
}
#endif
