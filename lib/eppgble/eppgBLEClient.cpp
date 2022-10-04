#include <Arduino.h>
#include <NimBLEDevice.h>
#include "eppgBLE.h"

#include <functional>
using namespace std::placeholders;

const char *SERVICE_UUID = "8877FB19-E00B-40BE-905E-ACB42C39E6B8";
const char *TH_CHAR_UUID = "5A57F691-C0B9-45DD-BDF1-279681212C29";
const char *STATUS_CHAR_UUID = "28913A56-5701-4B27-85DB-50985F224847";
static NimBLEClient *pClient = nullptr;
static QueueHandle_t bleQueue;

class ClientCallbacks : public NimBLEClientCallbacks {
  void onConnect(NimBLEClient* pClient) {
    Serial.println("Connected");
  }

  void onDisconnect(NimBLEClient* pClient) {
    Serial.print(pClient->getPeerAddress().toString().c_str());
    Serial.println("Disconnected");
  }

  bool onConnParamsUpdateRequest(NimBLEClient* pClient, const ble_gap_upd_params* params) {
    return true;
  }

  uint32_t onPassKeyRequest(){
    Serial.println("Client Passkey Request");
    return 123456;
  }

  bool onConfirmPIN(uint32_t pass_key){
    Serial.print("The passkey YES/NO number: ");
    Serial.println(pass_key);
    return true;
  }

  void onAuthenticationComplete(ble_gap_conn_desc* desc){
    if(!desc->sec_state.encrypted) {
      Serial.println("Encrypt connection failed - disconnecting");
      NimBLEDevice::getClientByID(desc->conn_handle)->disconnect();
      return;
    }
  }
};

class AdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
    Serial.print("Advertised Device found: ");
    Serial.println(advertisedDevice->toString().c_str());
    if(advertisedDevice->isAdvertisingService(NimBLEUUID(SERVICE_UUID)))
    {
      Serial.println("Found openppg service");
      NimBLEDevice::getScan()->stop();

      if (advertisedDevice->getAddress() != pClient->getPeerAddress() && !pClient->isConnected()) {
        pClient->setPeerAddress(advertisedDevice->getAddress());
      }
      unsigned long evt = bleEvent::DEVICE_FOUND;
      xQueueSendToBack(bleQueue, &evt, 0);
    }
  }
};

void clientTask(void * parameter) {
  EppgBLEClient *eppg = (EppgBLEClient*)parameter;

  for(;;) {
    unsigned long event;
    if(xQueueReceive( bleQueue, &event, pdMS_TO_TICKS(1000)) == pdPASS) {
      Serial.printf("proc evt: %u\n", event);
      eppg->processEvent(event);
    } else {
      if (!eppg->isConnected() && !eppg->isConnecting() &&
          !NimBLEDevice::getScan()->isScanning()) {
        if (NimBLEDevice::getScan()->start(0, nullptr, false)) {
          Serial.println("Started scan");
        } else {
          Serial.println("Failed to start scan");
        }
      }
    }
  }

  vTaskDelete(NULL);
}

void EppgBLEClient::processEvent(unsigned long event) {
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
    case bleEvent::DEVICE_FOUND:
      if (!this->connecting) {
        this->connecting = true;
        if (this->connect() && this->connectCB) {
          this->connectCB();
        }
      }
      break;
    default:
      break;
  }
}

bool EppgBLEClient::setThrottle(int val) {
  if (!pClient->isConnected()) {
    return false;
  }

  NimBLEAttValue v((uint8_t*)&val, sizeof(val));
  // Use write with response to verify.
  return pClient->setValue(NimBLEUUID(SERVICE_UUID), NimBLEUUID(TH_CHAR_UUID), v, true);
}

bool EppgBLEClient::isConnected() {
  return pClient->isConnected();
}

void EppgBLEClient::statusNotify(NimBLERemoteCharacteristic *pChar,
                                 uint8_t *pData, size_t length, bool isNotify) {
  if (length != sizeof(uint32_t)) {
    Serial.println("Invalid status data");
    return;
  }

  if (this->statusCB) {
    uint32_t val = *(uint32_t*)pData;
    this->statusCB(val);
  }
}

void EppgBLEClient::batteryNotify(NimBLERemoteCharacteristic *pChar,
                                  uint8_t *pData, size_t length, bool isNotify) {
  if (length != sizeof(uint8_t)) {
    Serial.println("Invalid battery data");
    return;
  }

  if (this->batteryCB) {
    uint8_t val = *pData;
    this->batteryCB(val);
  }
}

bool EppgBLEClient::disconnect() {
  if (pClient->isConnected()) {
    return pClient->disconnect() == 0;
  }
  return true;
}

bool EppgBLEClient::connect() {
  if (!pClient->isConnected()) {
    pClient->connect(false);
  }

  NimBLERemoteService* pMainSvc = nullptr;
  NimBLERemoteService* pBattSvc = nullptr;
  NimBLERemoteCharacteristic* pBattChr = nullptr;
  NimBLERemoteCharacteristic* pThChr = nullptr;
  NimBLERemoteCharacteristic* pStChr = nullptr;

  if (pClient->isConnected()) {
    pMainSvc = pClient->getService(SERVICE_UUID);
    if (pMainSvc) {
      pThChr = pMainSvc->getCharacteristic(TH_CHAR_UUID);
      pStChr = pMainSvc->getCharacteristic(STATUS_CHAR_UUID);
    }

    pBattSvc = pClient->getService("180F");
    if (pBattSvc) {
      pBattChr = pBattSvc->getCharacteristic("2A19");
    }
  }

  if (!pMainSvc || !pThChr || !pStChr || !pBattSvc || !pBattChr) {
    this->disconnect();
    this->connecting = false;
    return false;
  }

  if (!pStChr->subscribe(true, std::bind(&EppgBLEClient::statusNotify, this, _1, _2, _3, _4)) ||
      !pBattChr->subscribe(true, std::bind(&EppgBLEClient::batteryNotify, this, _1, _2, _3, _4))) {
    this->disconnect();
    this->connecting = false;
    return false;
  }

  this->connecting = false;
  return true;
}

void EppgBLEClient::begin() {
  Serial.println("Setting up BLE");
  NimBLEDevice::init("");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  pClient = NimBLEDevice::createClient();
  pClient->setClientCallbacks(new ClientCallbacks);
  NimBLEScan* pScan = NimBLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks);
  bleQueue = xQueueCreate(32, sizeof(unsigned long));
  xTaskCreate(clientTask, "clientTask", 5000, this, 2, NULL);
}