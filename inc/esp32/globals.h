// Copyright 2021 <Zach Whitehead>
#ifndef INC_GLOBALS_H_
#define INC_GLOBALS_H_

#include "../../inc/esp32/structs.h"

// Global functions

// main
bool   armSystem();
bool   disarmSystem();
void   handleArmFail();
void   removeCruise(bool alert);
void   setCruise();

const STR_ESC_TELEMETRY_140& getTelemetryData();

#endif  // INC_GLOBALS_H_
