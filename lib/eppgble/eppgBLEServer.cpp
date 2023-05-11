#include <Arduino.h>
#include <NimBLEDevice.h>
#include "eppgBLE.h"

static NimBLECharacteristic *pBattChr = nullptr;
static NimBLECharacteristic *pBmsVoltageChr = nullptr;
static NimBLECharacteristic *pThChr = nullptr;
static NimBLECharacteristic *pStChr = nullptr;
static NimBLECharacteristic *pArmChr = nullptr;
static NimBLECharacteristic *pTempChr = nullptr;
static NimBLECharacteristic *pBaroChr = nullptr;

static QueueHandle_t bleQueue;

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) {
    NimBLEAddress peerAddr(desc->peer_ota_addr);
    Serial.printf("Client connected, address: %s\n", peerAddr.toString().c_str());
#ifndef DISABLE_BLE_SECURITY
    // disconnect if already bonded to a handheld device and this peer is not bonded with us
    if (NimBLEDevice::getNumBonds() && !NimBLEDevice::isBonded(peerAddr)) {
      pServer->disconnect(desc->conn_handle);
    }
/*  Workaround NimBLE bug, security should be initiated by the client only for now
      else if (!desc->sec_state.encrypted) {
      NimBLEDevice::startSecurity(desc->conn_handle);
    }
*/
#else
    unsigned long evt = bleEvent::CONNECTED;
    xQueueSendToBack(bleQueue, &evt, 0);
#endif
  }

  void onDisconnect(NimBLEServer* pServer) {
    unsigned long evt = bleEvent::DISCONNECTED;
    xQueueSendToBack(bleQueue, &evt, 0);
  }

#ifndef DISABLE_BLE_SECURITY
  void onAuthenticationComplete(ble_gap_conn_desc* desc) {
    if(!desc->sec_state.encrypted) {
      NimBLEDevice::getServer()->disconnect(desc->conn_handle);
      Serial.println("Encrypt connection failed - disconnecting client");
      return;
    } else {
      // Change to advertising the main service UUID after pairing complete
      NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
      pAdvertising->reset();
      pAdvertising->removeServiceUUID(NimBLEUUID(PAIRING_AVAILABLE_UUID)); // workaround reset bug
      pAdvertising->addServiceUUID(MAIN_SERVICE_UUID);
      NimBLEDevice::whiteListAdd(NimBLEAddress(desc->peer_ota_addr));
      // only allow connections from bonded peers
      pAdvertising->setScanFilter(true, true);
      unsigned long evt = bleEvent::CONNECTED;
      xQueueSendToBack(bleQueue, &evt, 0);
    }
    Serial.println("Authentication Complete");
  }
#endif
};

class throttleChrCallbacks: public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pCharacteristic) {
    if (pCharacteristic == pThChr) {
      unsigned long evt = bleEvent::THROTTLE_UPDATE;
      xQueueSendToBack(bleQueue, &evt, 0);
    }
  }
};

class armChrCallbacks: public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pCharacteristic) {
    if (pCharacteristic == pArmChr) {
      uint8_t val = pCharacteristic->getValue<uint8_t>();
      if (val) {
        unsigned long evt = bleEvent::ARM;
        xQueueSendToBack(bleQueue, &evt, 0);
      } else {
        unsigned long evt = bleEvent::DISARM;
        xQueueSendToBack(bleQueue, &evt, 0);
      }
    }
  }
};

void serverTask(void * parameter) {
  EppgBLEServer *eppg = (EppgBLEServer*)parameter;

  for(;;) {
    unsigned long event;
    if(xQueueReceive( bleQueue, &event, pdMS_TO_TICKS(1000)) == pdPASS) {
      Serial.printf("proc evt: %u\n", event);
      eppg->processEvent(event);
    } else {
      if (!NimBLEDevice::getServer()->getConnectedCount() &&
          !NimBLEDevice::getAdvertising()->isAdvertising()) {
        if (NimBLEDevice::getAdvertising()->start()) {
          Serial.println("Advertising started");
        } else {
          Serial.println("Failed to start advertising");
        }
      } else {
        //pStChr->notify();
        //pBattChr->notify();
      }
    }
  }

  vTaskDelete(NULL);
}

void EppgBLEServer::processEvent(unsigned long event) {
  switch(event) {
    case bleEvent::CONNECTED:
      if (this->connectCB) {
        this->connectCB();
      }
      break;
    case bleEvent::DISCONNECTED:
      if (this->disconnectCB) {
        this->disconnectCB();
      }
      break;
    case bleEvent::THROTTLE_UPDATE:
      if (this->throttleCB) {
        this->throttleCB(pThChr->getValue<int>());
      }
      break;
    case bleEvent::ARM:
      if (this->armCB) {
        this->armCB();
      }
      break;
    case bleEvent::DISARM:
      if (this->disarmCB){
        this->disarmCB();
      }
      break;
    default:
      break;
  }
}

#ifdef BLE_LATENCY_TEST
void EppgBLEServer::setStatus(latency_test_t &lat) {
  pStChr->setValue(lat);
  pStChr->notify();
}
#else
void EppgBLEServer::setStatus(uint32_t val) {
  pStChr->setValue(val);
  pStChr->notify();
}
#endif

void EppgBLEServer::setBattery(uint8_t val) {
  pBattChr->setValue(val);
  pBattChr->notify();
}

void EppgBLEServer::setTemp(double temp) {
  pTempChr->setValue<int16_t>(temp * 100);
}

void EppgBLEServer::setBmp(double pressure) {
  pBaroChr->setValue<uint32_t>((pressure + 0.05F) * 10);
}

void EppgBLEServer::setBmsVoltage(float voltage) {
  pBmsVoltageChr->setValue<uint32_t>(voltage * 100.0F);
  pBmsVoltageChr->notify();
}

void EppgBLEServer::begin() {
  Serial.println("Setting up BLE");
  NimBLEDevice::init("OpenPPG Controller");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  NimBLEDevice::setSecurityAuth(true, false, false); // enable bonding, no MITM, no SC

  NimBLEServer *pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks);
  NimBLEService *pMainSvc = pServer->createService(MAIN_SERVICE_UUID);
  pThChr = pMainSvc->createCharacteristic(TH_CHAR_UUID,
                                       NIMBLE_PROPERTY::READ     |
                                       NIMBLE_PROPERTY::WRITE    |
                                       NIMBLE_PROPERTY::NOTIFY);
  pThChr->setCallbacks(new throttleChrCallbacks);

  pArmChr = pMainSvc->createCharacteristic(ARM_CHAR_UUID,
                                      NIMBLE_PROPERTY::READ     |
                                      NIMBLE_PROPERTY::WRITE    |
                                      NIMBLE_PROPERTY::NOTIFY, 1);
  pArmChr->setCallbacks(new armChrCallbacks);

  pStChr = pMainSvc->createCharacteristic(STATUS_CHAR_UUID,
                                       NIMBLE_PROPERTY::READ |
                                       NIMBLE_PROPERTY::NOTIFY);

  NimBLEService *pBattSvc = pServer->createService(BATT_SERVICE_UUID);
  pBattChr = pBattSvc->createCharacteristic(BATT_CHAR_UUID,
                                            NIMBLE_PROPERTY::READ |
                                            NIMBLE_PROPERTY::NOTIFY, 1);

  NimBLEService *pBmsSvc = pServer->createService(BMS_SERVICE_UUID);
  pBmsVoltageChr = pBmsSvc->createCharacteristic(BMS_VOLTAGE_CHAR_UUID,
                                            NIMBLE_PROPERTY::READ |
                                            NIMBLE_PROPERTY::NOTIFY, 2);


  NimBLEService *pEnvService = pServer->createService(ENV_SERVICE_UUID);
  pTempChr = pEnvService->createCharacteristic(TEMP_CHAR_UUID, NIMBLE_PROPERTY::READ, 2);
  pBaroChr = pEnvService->createCharacteristic(BARO_CHAR_UUID, NIMBLE_PROPERTY::READ, 4);

  pMainSvc->start();
  pBattSvc->start();
  pEnvService->start();

  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
#ifndef DISABLE_BLE_SECURITY
  // If not bonded with a handheld device, advertise the pairing available UUID
  if (!NimBLEDevice::getNumBonds()) {
    pAdvertising->addServiceUUID(PAIRING_AVAILABLE_UUID);
  } else {
    pAdvertising->addServiceUUID(pMainSvc->getUUID());
    // Add bonded devices to the whitelist
    int bondCount = NimBLEDevice::getNumBonds();
    for (int i = 0; i < bondCount; i++) {
      NimBLEDevice::whiteListAdd(NimBLEDevice::getBondedAddress(i));
    }
    pAdvertising->setScanFilter(true, true); // only allow connections from bonded devices
  }
#else
  pAdvertising->addServiceUUID(pMainSvc->getUUID());
#endif
  pAdvertising->setScanResponse(false);

  bleQueue = xQueueCreate(32, sizeof(unsigned long));
  xTaskCreate(serverTask, "serverTask", 5000, this, 2, NULL);
}

