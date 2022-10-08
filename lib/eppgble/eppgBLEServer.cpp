#include <Arduino.h>
#include <NimBLEDevice.h>
#include "eppgBLE.h"

const char *SERVICE_UUID = "8877FB19-E00B-40BE-905E-ACB42C39E6B8";
const char *TH_CHAR_UUID = "5A57F691-C0B9-45DD-BDF1-279681212C29";
const char *STATUS_CHAR_UUID = "28913A56-5701-4B27-85DB-50985F224847";
const char *ARM_CHAR_UUID = "aeb83642-7cc4-45e9-bdf9-c9450876d98d";

static NimBLECharacteristic *pBattChr = nullptr;
static NimBLECharacteristic *pThChar = nullptr;
static NimBLECharacteristic *pStChar = nullptr;
static NimBLECharacteristic *pArmChar = nullptr;
static NimBLECharacteristic *pTempChar = nullptr;
static NimBLECharacteristic *pBarChar = nullptr;

static QueueHandle_t bleQueue;

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) {
    Serial.print("Client connected, address: ");
    Serial.println(NimBLEAddress(desc->peer_ota_addr).toString().c_str());
    unsigned long evt = bleEvent::CONNECTED;
    xQueueSendToBack(bleQueue, &evt, 0);
  }

  void onDisconnect(NimBLEServer* pServer) {
    unsigned long evt = bleEvent::DISCONNECTED;
    xQueueSendToBack(bleQueue, &evt, 0);
  }

  uint32_t onPassKeyRequest() {
    Serial.println("Server Passkey Request");
    return 123456;
  }

  bool onConfirmPIN(uint32_t pass_key) {
    Serial.print("The passkey YES/NO number: ");Serial.println(pass_key);
    return true;
  }

  void onAuthenticationComplete(ble_gap_conn_desc* desc) {
    if(!desc->sec_state.encrypted) {
      NimBLEDevice::getServer()->disconnect(desc->conn_handle);
      Serial.println("Encrypt connection failed - disconnecting client");
      return;
    }
    Serial.println("Authentication Complete");
  }
};

class throttleChrCallbacks: public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pCharacteristic) {
    if (pCharacteristic == pThChar) {
      unsigned long evt = bleEvent::THROTTLE_UPDATE;
      xQueueSendToBack(bleQueue, &evt, 0);
    }
  }
};

class armChrCallbacks: public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pCharacteristic) {
    if (pCharacteristic == pArmChar) {
      if (pCharacteristic->getValue<bool>()) {
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
        //pStChar->notify();
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
        this->throttleCB(pThChar->getValue<int>());
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
  pStChar->setValue(lat);
  pStChar->notify();
}
#else
void EppgBLEServer::setStatus(uint32_t val) {
  pStChar->setValue(val);
  pStChar->notify();
}
#endif

void EppgBLEServer::setBattery(uint8_t val) {
  pBattChr->setValue(val);
  pBattChr->notify();
}

void EppgBLEServer::begin() {
  Serial.println("Setting up BLE");
  NimBLEDevice::init("OpenPPG Controller");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  NimBLEServer *pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks);
  NimBLEService *pMainSvc = pServer->createService(SERVICE_UUID);
  pThChar = pMainSvc->createCharacteristic(TH_CHAR_UUID,
                                       NIMBLE_PROPERTY::READ     |
                                       NIMBLE_PROPERTY::WRITE    |
                                       NIMBLE_PROPERTY::NOTIFY);
  pThChar->setCallbacks(new throttleChrCallbacks);

  pArmChar = pMainSvc->createCharacteristic(ARM_CHAR_UUID,
                                      NIMBLE_PROPERTY::READ     |
                                      NIMBLE_PROPERTY::WRITE    |
                                      NIMBLE_PROPERTY::NOTIFY);
  pArmChar->setCallbacks(new armChrCallbacks);

  pStChar = pMainSvc->createCharacteristic(STATUS_CHAR_UUID,
                                       NIMBLE_PROPERTY::READ |
                                       NIMBLE_PROPERTY::NOTIFY);

  NimBLEService *pBattSvc = pServer->createService("180F");
  pBattChr = pBattSvc->createCharacteristic("2A19", NIMBLE_PROPERTY::READ |
                                                    NIMBLE_PROPERTY::NOTIFY, 1);

  NimBLEService *pEnvService = pServer->createService("181A");
  pTempChar = pEnvService->createCharacteristic("2A6E", NIMBLE_PROPERTY::READ, 2);
  pBarChar = pEnvService->createCharacteristic("2A6D", NIMBLE_PROPERTY::READ,4);

  pMainSvc->start();
  pBattSvc->start();
  pEnvService->start();

  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(pMainSvc->getUUID());
  pAdvertising->setScanResponse(false);

  bleQueue = xQueueCreate(32, sizeof(unsigned long));
  xTaskCreate(serverTask, "serverTask", 5000, this, 2, NULL);
}

