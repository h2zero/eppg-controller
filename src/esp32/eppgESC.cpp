#ifndef ESC_TELEM_DISABLED

#include <Arduino.h>
#include "eppgESC.h"
#include <CircularBuffer.h>
#include "../../inc/esp32/globals.h"
#include "eppgPower.h"

extern STR_DEVICE_DATA_140_V1 deviceData;
static STR_ESC_TELEMETRY_140 telemetryData;

const STR_ESC_TELEMETRY_140& getTelemetryData() {
  return telemetryData;
}

void handleTelemetryTask(void * param) {
  EppgEsc *esc = (EppgEsc*)param;
  for (;;) {
    esc->handleTelemetry();
    delay(50);
  }

  vTaskDelete(NULL); // should never reach this
}

EppgEsc::EppgEsc()
: escData{0},
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
  xTaskCreate(handleTelemetryTask, "handleTelemetry", 5000, this, 1, NULL);
  trackPowerTaskStart();
}

void EppgEsc::serialRead() {  // TODO needed?
  while (SerialESC.available() > 0) {
    SerialESC.read();
  }
}

void EppgEsc::handleTelemetry() {
  Serial.println(F("Handling Telemetry"));
  serialRead();
  SerialESC.readBytes(escData, ESC_DATA_V2_SIZE);
  parseData(escData);
}

// run checksum and return true if valid
// new v2
int EppgEsc::checkFlectcher16(byte byteBuffer[]) {
  int fCCRC16;
  int i;
  int c0 = 0;
  int c1 = 0;

  // Calculate checksum intermediate bytesUInt16
  for (i = 0; i < 18; i++) //Check only first 18 bytes, skip crc bytes
  {
      c0 = (int)(c0 + ((int)byteBuffer[i])) % 255;
      c1 = (int)(c1 + c0) % 255;
  }
  // Assemble the 16-bit checksum value
  fCCRC16 = ( c1 << 8 ) | c0;
  return (int)fCCRC16;
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


void EppgEsc::parseData(byte buffer[]) {
  // if(sizeof(buffer) != 22) {
  //     Serial.print("wrong size ");
  //     Serial.println(sizeof(buffer));
  //     return; //Ignore malformed packets
  // }

  if (buffer[20] != 255 || buffer[21] != 255) {
    Serial.println("no stop byte");

    return; //Stop byte of 65535 not recieved
  }

  //Check the fletcher checksum
  int checkFletch = checkFlectcher16(buffer);

  // checksum
  int checkCalc = (int)(((buffer[19] << 8) + buffer[18]));

  //TODO alert if no new data in 3 seconds
  // Checksums do not match
  if (checkFletch != checkCalc) {
    return;
  }
  // Voltage
  float voltage = (buffer[1] << 8 | buffer[0]) / 100.0;
  telemetryData.volts = voltage; //Voltage

  if (telemetryData.volts > BATT_MIN_V) {
    telemetryData.volts += 1.0; // calibration
  }

  if (telemetryData.volts > 1) {  // ignore empty data
    pushVoltage(telemetryData.volts);
  }

  // Temperature
  float rawVal = (float)((buffer[3] << 8) + buffer[2]);

  static int SERIESRESISTOR = 10000;
  static int NOMINAL_RESISTANCE = 10000;
  static int NOMINAL_TEMPERATURE = 25;
  static int BCOEFFICIENT = 3455;

  //convert value to resistance
  float Rntc = (4096 / (float) rawVal) - 1;
  Rntc = SERIESRESISTOR / Rntc;

  // Get the temperature
  float temperature = Rntc / (float) NOMINAL_RESISTANCE; // (R/Ro)
  temperature = (float) log(temperature); // ln(R/Ro)
  temperature /= BCOEFFICIENT; // 1/B * ln(R/Ro)

  temperature += 1.0 / ((float) NOMINAL_TEMPERATURE + 273.15); // + (1/To)
  temperature = 1.0 / temperature; // Invert
  temperature -= 273.15; // convert to Celcius

  // filter bad values
  if (temperature < 0 || temperature > 200) {
    temperature = 0;
  }

  temperature = (float) trunc(temperature * 100) / 100; // 2 decimal places
  telemetryData.temperatureC = temperature;

  // Current
  int currentAmpsInput = (int)((buffer[5] << 8) + buffer[4]);
  telemetryData.amps = (currentAmpsInput / 12.5); //Input current

  // Serial.print("amps ");
  // Serial.print(currentAmpsInput);
  // Serial.print(" - ");

  setWatts(telemetryData.amps * telemetryData.volts);

  // eRPM
  int poleCount = 62;
  int currentERPM = (int)((buffer[11] << 24) + (buffer[10] << 16) + (buffer[9] << 8) + (buffer[8] << 0)); //ERPM output
  int currentRPM = currentERPM / poleCount;  // Real RPM output
  telemetryData.eRPM = currentRPM;

  // Serial.print("RPM ");
  // Serial.print(currentRPM);
  // Serial.print(" - ");

  // Input Duty
  int throttleDuty = (int)(((buffer[13] << 8) + buffer[12]) / 10);
  telemetryData.inPWM = (throttleDuty / 10); //Input throttle

  // Serial.print("throttle ");
  // Serial.print(telemetryData.inPWM);
  // Serial.print(" - ");

  // Motor Duty
  int motorDuty = (int)(((buffer[15] << 8) + buffer[14]) / 10);
  int currentMotorDuty = (motorDuty / 10); //Motor duty cycle

  /* Status Flags
  # Bit position in byte indicates flag set, 1 is set, 0 is default
  # Bit 0: Motor Started, set when motor is running as expected
  # Bit 1: Motor Saturation Event, set when saturation detected and power is reduced for desync protection
  # Bit 2: ESC Over temperature event occuring, shut down method as per configuration
  # Bit 3: ESC Overvoltage event occuring, shut down method as per configuration
  # Bit 4: ESC Undervoltage event occuring, shut down method as per configuration
  # Bit 5: Startup error detected, motor stall detected upon trying to start*/
  telemetryData.statusFlag = buffer[16];
  // Serial.print("status ");
  // Serial.print(raw_telemdata.statusFlag, BIN);
  // Serial.print(" - ");
  // Serial.println(" ");

}

#endif // ESC_TELEM_DISABLED