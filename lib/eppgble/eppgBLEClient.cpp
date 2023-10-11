#include <Arduino.h>
#include <NimBLEDevice.h>
#include "eppgBLE.h"

#include <functional>
using namespace std::placeholders;

static NimBLEClient *pClient = nullptr;
static QueueHandle_t bleQueue;

#ifndef EPPG_BLE_CONN_INTERVAL
  #define EPPG_BLE_CONN_INTERVAL 6 // 7.5ms
#endif


class ClientCallbacks : public NimBLEClientCallbacks {
  void onConnect(NimBLEClient* pClient) {
#ifndef DISABLE_BLE_SECURITY
    if (pClient->getConnInfo().isEncrypted()) {
      unsigned long evt = bleEvent::CONNECTED;
      xQueueSendToBack(bleQueue, &evt, 0);
    } else {
      pClient->secureConnection();
    }
#else
    unsigned long evt = bleEvent::CONNECTED;
    xQueueSendToBack(bleQueue, &evt, 0);
#endif
  }

  void onDisconnect(NimBLEClient* pClient) {
    unsigned long evt = bleEvent::DISCONNECTED;
    xQueueSendToBack(bleQueue, &evt, 0);
  }

  bool onConnParamsUpdateRequest(NimBLEClient* pClient, const ble_gap_upd_params* params) {
    return true;
  }

#ifndef DISABLE_BLE_SECURITY
  void onAuthenticationComplete(ble_gap_conn_desc* desc) {
    if(!desc->sec_state.encrypted) {
      Serial.println("Encrypt connection failed - disconnecting");
      NimBLEDevice::getClientByID(desc->conn_handle)->disconnect();
      return;
    }

    // Add bonded peer to whitelist
    NimBLEDevice::whiteListAdd(NimBLEAddress(desc->peer_ota_addr));
    // Set scan filter to only process bonded peers added to the whitelist
    NimBLEDevice::getScan()->setFilterPolicy(BLE_HCI_SCAN_FILT_USE_WL);
    Serial.println("Authentication Complete");
    unsigned long evt = bleEvent::CONNECTED;
    xQueueSendToBack(bleQueue, &evt, 0);
  }
#endif
};

class AdvertisedDeviceCallbacks: public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
    Serial.print("Advertised Device found: ");
    Serial.println(advertisedDevice->toString().c_str());

#ifndef DISABLE_BLE_SECURITY
    // Precautionary - Do not connect if already bonded to a hub and it is not this one.
    if (NimBLEDevice::getNumBonds() && !NimBLEDevice::isBonded(advertisedDevice->getAddress())) {
      return;
    }
#endif

    // Only connect to devices advertising the openppg service or pairing available service when not bonded
    if (advertisedDevice->isAdvertisingService(NimBLEUUID(MAIN_SERVICE_UUID))
#ifndef DISABLE_BLE_SECURITY
        || (advertisedDevice->isAdvertisingService(NimBLEUUID(PAIRING_AVAILABLE_UUID))
        && !NimBLEDevice::getNumBonds())
#endif
    ) {
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
      //Serial.printf("proc evt: %u\n", event);
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

bool EppgBLEClient::arm() {
  if (!this->isConnected()) {
    return false;
  }

  uint8_t val = 0x01;
  NimBLEAttValue v((uint8_t*)&val, sizeof(val));

  // Use write with response to verify.
  return pClient->setValue(NimBLEUUID(MAIN_SERVICE_UUID), NimBLEUUID(ARM_CHAR_UUID), v, true);
}

bool EppgBLEClient::disarm() {
  if (!this->isConnected()) {
    return false;
  }

  uint8_t val = 0x00;
  NimBLEAttValue v((uint8_t*)&val, sizeof(val));

  // Use write with response to verify.
  return pClient->setValue(NimBLEUUID(MAIN_SERVICE_UUID), NimBLEUUID(ARM_CHAR_UUID), v, true);
}

bool EppgBLEClient::setThrottle(int val) {
  if (!this->isConnected()) {
    return false;
  }

  NimBLEAttValue v((uint8_t*)&val, sizeof(val));
  // Use write with response to verify.
  return pClient->setValue(NimBLEUUID(MAIN_SERVICE_UUID), NimBLEUUID(TH_CHAR_UUID), v, true);
}

float EppgBLEClient::getTemp() {
  if (!this->isConnected()) {
    return 0;
  }
  NimBLEAttValue v = pClient->getValue(NimBLEUUID(ENV_SERVICE_UUID), NimBLEUUID(TEMP_CHAR_UUID));
  return (float)(v.getValue<int16_t>() / 100.0F);
}

float EppgBLEClient::getBmp() {
  if (!this->isConnected()) {
    return 0;
  }
  NimBLEAttValue v = pClient->getValue(NimBLEUUID(ENV_SERVICE_UUID), NimBLEUUID(BARO_CHAR_UUID));
  return (float)(v.getValue<uint32_t>() / 10.0F);
}

const STR_BMS_DATA& EppgBLEClient::getBmsData() {
  if (this->isConnected()) {
    NimBLEAttValue v = pClient->getValue(NimBLEUUID(BMS_SERVICE_UUID), NimBLEUUID(BMS_CHAR_UUID));
    m_bmsData = v.getValue<STR_BMS_DATA>();
  }

  return m_bmsData;
}

bool EppgBLEClient::isConnected() {
  return this->connecting ? false : pClient->isConnected();
}

void EppgBLEClient::statusNotify(NimBLERemoteCharacteristic *pChar,
                                 uint8_t *pData, size_t length, bool isNotify) {
#ifndef BLE_LATENCY_TEST
  if (length != sizeof(uint32_t)) {
    Serial.println("Invalid status data");
    return;
  }
#endif

  if (this->statusCB) {
#ifdef BLE_LATENCY_TEST
    latency_test_t val = *(latency_test_t*)pData;
    this->statusCB(val, pClient->getRssi());
#else
    uint32_t val = *(uint32_t*)pData;
    this->statusCB(val);
#endif
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
    pMainSvc = pClient->getService(MAIN_SERVICE_UUID);
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
    Serial.println("Failed to get required device attributes");
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
  NimBLEDevice::setSecurityAuth(true, false, false);  // enable bonding, no MITM, no SC

  pClient = NimBLEDevice::createClient();
  pClient->setClientCallbacks(new ClientCallbacks);
  pClient->setConnectionParams(EPPG_BLE_CONN_INTERVAL, EPPG_BLE_CONN_INTERVAL, 0, 200);

  NimBLEScan* pScan = NimBLEDevice::getScan();
  pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks);

#ifndef DISABLE_BLE_SECURITY
  // If bonded with any hub devices add them to the scan whitelist
  int bondCount = NimBLEDevice::getNumBonds();
  for (int i = 0; i < bondCount; i++) {
    NimBLEDevice::whiteListAdd(NimBLEDevice::getBondedAddress(i));
  }

  // Set scan filter to only process bonded peers added to the whitelist
  if (bondCount) {
    pScan->setFilterPolicy(BLE_HCI_SCAN_FILT_USE_WL);
  }
#endif

  bleQueue = xQueueCreate(32, sizeof(unsigned long));
  xTaskCreate(clientTask, "clientTask", 5000, this, 2, NULL);
}
