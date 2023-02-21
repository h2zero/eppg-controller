#include <Arduino.h>
#include "eppgSensors.h"
#include "../../inc/eppgConfig.h"
#include "../../inc/esp32/globals.h"  // device config

#ifdef EPPG_BLE_HANDHELD
#include <eppgBLE.h>
extern EppgBLEClient ble;
#else
static Adafruit_BMP3XX bmp;
static SFE_UBLOX_GNSS myGNSS;
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

// Start the Gps sensor
void initGps() {
#ifndef EPPG_BLE_HANDHELD
  if (myGNSS.begin() == false) //Connect to the u-blox module using Wire port
  {
    Serial.println(F("u-blox GNSS not detected at default I2C address."));
    // TODO handle error
  }
  Serial.println("ran gps init");
#endif
}
long lastTime = 0; //Simple local timer. Limits amount if I2C traffic to u-blox module.

// Start the bmp388 sensor
void debugGps() {
#ifndef EPPG_BLE_HANDHELD
  if (millis() - lastTime > 1000)
  {
    lastTime = millis(); //Update the timer
    long latitude = myGNSS.getLatitude();
    Serial.print(F("Lat: "));
    Serial.print(latitude);

    long longitude = myGNSS.getLongitude();
    Serial.print(F(" Long: "));
    Serial.print(longitude);

    long speed = myGNSS.getGroundSpeed();
    Serial.print(F(" Speed: "));
    Serial.print(speed);
    Serial.print(F(" (mm/s)"));

    long heading = myGNSS.getHeading();
    Serial.print(F(" Heading: "));
    Serial.print(heading);
    Serial.print(F(" (degrees * 10^-5)"));

    long altitude = myGNSS.getAltitude();
    Serial.print(F(" Alt: "));
    Serial.print(altitude);
    Serial.print(F(" (mm)"));
    
    byte fixType = myGNSS.getFixType();
    Serial.print(F(" Fix: "));
    if(fixType == 0) Serial.print(F("No fix"));
    else if(fixType == 2) Serial.print(F("2D"));
    else if(fixType == 3) Serial.print(F("3D"));
    else if(fixType == 5) Serial.print(F("Time only"));

    Serial.println();
  }
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