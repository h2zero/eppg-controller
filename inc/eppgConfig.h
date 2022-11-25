#ifndef INC_EPPG_CONFIG_H_
#define INC_EPPG_CONFIG_H_

#ifdef M0_PIO
  #include "sp140/m0-config.h"          // device config
#elif RP_PIO
  #include "sp140/rp2040-config.h"         // device config
#elif ESP_PLATFORM
  #include "esp32/esp32-config.h"
#endif

#endif // INC_EPPG_CONFIG_H_
