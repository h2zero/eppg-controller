#ifndef ESC_DISABLED

#include <Arduino.h>
#include "eppgESC.h"
#include <CircularBuffer.h>
#include "../../inc/esp32/structs.h"
#include "eppgPower.h"

extern CircularBuffer<float, 50> voltageBuffer;
extern STR_DEVICE_DATA_140_V1 deviceData;
extern STR_ESC_TELEMETRY_140 telemetryData;

EppgEsc::EppgEsc()
: escData{0},
  transmitted(0),
  failed(0),
  _volts(0),
  _temperatureC(0),
  _amps(0),
  _eRPM(0),
  _inPWM(0),
  _outPWM(0) {
}

void EppgEsc::begin() {
  SerialESC.begin(ESC_BAUD_RATE);
  SerialESC.setTimeout(ESC_TIMEOUT);
}

void EppgEsc::serialRead() {  // TODO needed?
  while (SerialESC.available() > 0) {
    SerialESC.read();
  }
}

void EppgEsc::handleTelemetry() {
  serialRead();
  SerialESC.readBytes(escData, ESC_DATA_SIZE);
  if (enforceFletcher16()) {
    parseData();
  }
  // printRawSentence();
}

// run checksum and return true if valid
bool EppgEsc::enforceFletcher16() {
  // Check checksum, revert to previous data if bad:
  word checksum = (unsigned short)(word(escData[19], escData[18]));
  unsigned char sum1 = 0;
  unsigned char sum2 = 0;
  unsigned short sum = 0;
  for (int i = 0; i < ESC_DATA_SIZE-2; i++) {
    sum1 = (unsigned char)(sum1 + escData[i]);
    sum2 = (unsigned char)(sum2 + sum1);
  }
  sum = (unsigned char)(sum1 - sum2);
  sum = sum << 8;
  sum |= (unsigned char)(sum2 - 2*sum1);
  // Serial.print(F("     SUM: "));
  // Serial.println(sum);
  // Serial.print(sum1,HEX);
  // Serial.print(" ");
  // Serial.println(sum2,HEX);
  // Serial.print(F("CHECKSUM: "));
  // Serial.println(checksum);
  if (sum != checksum) {
    //Serial.println(F("_____________________CHECKSUM FAILED!"));
    failed++;
    if (failed >= 1000) {  // keep track of how reliable the transmission is
      transmitted = 1;
      failed = 0;
    }
    return false;
  }
  return true;
}

// for debugging
void EppgEsc::printRawSentence() {
  Serial.print(F("DATA: "));
  for (int i = 0; i < ESC_DATA_SIZE; i++) {
    Serial.print(escData[i], HEX);
    Serial.print(F(" "));
  }
  Serial.println();
}


void EppgEsc::parseData() {
  // LSB First
  // TODO is this being called even with no ESC?

  _volts = word(escData[1], escData[0]);
  //_volts = ((unsigned int)escData[1] << 8) + escData[0];
  telemetryData.volts = _volts / 100.0;

  if (telemetryData.volts > BATT_MIN_V) {
    telemetryData.volts += 1.5;  // calibration
  }

  if (telemetryData.volts > 1) {  // ignore empty data
    voltageBuffer.push(telemetryData.volts);
  }

  // Serial.print(F("Volts: "));
  // Serial.println(telemetryData.volts);

  // batteryPercent = mapd(telemetryData.volts, BATT_MIN_V, BATT_MAX_V, 0.0, 100.0); // flat line

  _temperatureC = word(escData[3], escData[2]);
  telemetryData.temperatureC = _temperatureC/100.0;
  // reading 17.4C = 63.32F in 84F ambient?
  // Serial.print(F("TemperatureC: "));
  // Serial.println(temperatureC);

  _amps = word(escData[5], escData[4]);
  telemetryData.amps = _amps;

  // Serial.print(F("Amps: "));
  // Serial.println(amps);

  setWatts(telemetryData.amps * telemetryData.volts);

  // 7 and 6 are reserved bytes

  _eRPM = escData[11];     // 0
  _eRPM << 8;
  _eRPM += escData[10];    // 0
  _eRPM << 8;
  _eRPM += escData[9];     // 30
  _eRPM << 8;
  _eRPM += escData[8];     // b4
  telemetryData.eRPM = _eRPM/6.0/2.0;

  // Serial.print(F("eRPM: "));
  // Serial.println(eRPM);

  _inPWM = word(escData[13], escData[12]);
  telemetryData.inPWM = _inPWM/100.0;

  // Serial.print(F("inPWM: "));
  // Serial.println(inPWM);

  _outPWM = word(escData[15], escData[14]);
  telemetryData.outPWM = _outPWM/100.0;

  // Serial.print(F("outPWM: "));
  // Serial.println(outPWM);

  // 17 and 16 are reserved bytes
  // 19 and 18 is checksum
  telemetryData.checksum = word(escData[19], escData[18]);

  // Serial.print(F("CHECKSUM: "));
  // Serial.print(escData[19]);
  // Serial.print(F(" + "));
  // Serial.print(escData[18]);
  // Serial.print(F(" = "));
  // Serial.println(checksum);
}

#endif // ESC_DISABLED