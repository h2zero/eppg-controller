#include <Arduino.h>
#include "eppgSensors.h"
#include "../../inc/eppgConfig.h"
#include "../../inc/esp32/globals.h"  // device config

#ifdef EPPG_BLE_HANDHELD
#include <eppgBLE.h>
extern EppgBLEClient ble;
#else
static Adafruit_BMP3XX bmp;
#endif

extern STR_DEVICE_DATA_140_V1 deviceData;

// Start the bmp388 sensor
void initBmp() {
#ifndef EPPG_BLE_HANDHELD
  bmp.begin_I2C();
  bmp.setOutputDataRate(BMP3_ODR_25_HZ);
  bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_2X);
  bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
  bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_15);
#endif
}

// Get the baro pressure reading from bmp3xx
float getPressure() {
#ifndef EPPG_BLE_HANDHELD
  if (! bmp.performReading()) {
    Serial.println("Failed to perform reading :(");
    return 0.0;
  }
  return bmp.pressure; // /100 for hPa
#endif
}

#define SEALEVELPRESSURE_HPA (1013.25)
// convert barometer data to altitude in meters
float getAltitudeM() { 
#ifdef EPPG_BLE_HANDHELD // Only should be called in handheld
  float atmospheric = ble.getBmp() / 100.0F;
  float altitudeM = 44330.0 * (1.0 - pow(atmospheric / deviceData.sea_pressure, 0.1903));
  return altitudeM;
#endif
}

// Get the temperature reading from bmp3xx
float getAmbientTempC() {
#ifdef EPPG_BLE_HANDHELD
  return ble.getTemp();
#else
  return bmp.temperature;
#endif
}