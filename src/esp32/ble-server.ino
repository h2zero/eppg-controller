#if defined(EPPG_BLE_SERVER)

void bleConnected(){Serial.println("Client Connected");}
void bleDisconnected() {Serial.println("Client Disconnected");}
void bleThrottleUpdate(int val) {
#ifdef BLE_TEST
  Serial.printf("Updated Throttle: %d\n", val);
#else
  esc.writeMicroseconds(val);
#endif
}

void bleArm() {
#ifdef BLE_TEST
  Serial.println("Armed from BLE");
#else
  esc.writeMicroseconds(ESC_DISARMED_PWM);
#endif
}

void bleDisarm() {
#ifdef BLE_TEST
  Serial.println("Disarmed from BLE");
#else
  esc.writeMicroseconds(ESC_DISARMED_PWM);
#endif
}

void handleTelemetryTask(void * parameter) {
  for (;;) {
    handleTelemetry();
#ifndef BLE_TEST
    bmp.performReading();
    ble.setTemp(bmp.temperature);
    ble.setBmp(bmp.pressure);
#else
    double temp = random(1, 50);
    temp += (random(1, 99) / 100.0F);
    ble.setTemp(temp);
    double pressure = random(300, 0xFFFF);
    pressure += (random(1, 99) / 100.0F);
    ble.setBmp(pressure);
#endif
    delay(50);
  }

  vTaskDelete(NULL); // should never reach this
}

void trackPowerTask(void * parameter) {
  for (;;) {
    trackPower();
    delay(250);
  }

  vTaskDelete(NULL); // should never reach this
}

void setupBleServer() {
  SerialESC.begin(ESC_BAUD_RATE);
  SerialESC.setTimeout(ESC_TIMEOUT);

  ble.setConnectCallback(bleConnected);
  ble.setDisconnectCallback(bleDisconnected);
  ble.setThrottleCallback(bleThrottleUpdate);
  ble.setArmCallback(bleArm);
  ble.setDisarmCallback(bleDisarm);
  ble.begin();

  esc.attach(ESC_PIN);
  esc.writeMicroseconds(ESC_DISARMED_PWM);

  xTaskCreate(trackPowerTask, "trackPower", 5000, NULL, 1, NULL);
  xTaskCreate(handleTelemetryTask, "handleTelemetry", 5000, NULL, 1, NULL);
}

void bleServerLoop() {
#if defined(BLE_TEST)
    ble.setBattery(random(0x00,0x64));
  #ifdef BLE_LATENCY_TEST
    ble_lat_test.count++;
    ble_lat_test.time = millis();
    ble.setStatus(ble_lat_test);
  #else
    ble.setStatus(random(0x00,0xFFFF));
  #endif
  delay(1000);
#endif
}

#endif // EPPG_BLE_SERVER
