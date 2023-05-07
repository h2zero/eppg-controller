// Copyright 2021 <Zach Whitehead>
#ifndef INC_ESP32_CONFIG_H_
#define INC_ESP32_CONFIG_H_

#include "shared-config.h"

#if defined(EPPG_BLE_HUB)
#include "esp-hub-config.h"
#else 
#include "esp-controller-config.h"
#endif

#define ENABLE_VIB            false    // enable vibration

#endif  // INC_ESP32_CONFIG_H_
