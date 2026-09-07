#pragma once

#include <Arduino.h>

#ifndef USE_BLE_TELEMETRY
#define USE_BLE_TELEMETRY 0
#endif

#define WDF_BLE_SERVICE_UUID        "96F10C00-6DF1-4C00-8000-00805F9B34FB"
#define WDF_BLE_CHARACTERISTIC_TX   "96F10C01-6DF1-4C00-8000-00805F9B34FB"

#if USE_BLE_TELEMETRY
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

namespace WhereDaFlockBLETelemetry {

static BLEServer* pServer = nullptr;
static BLECharacteristic* pTxCharacteristic = nullptr;
static bool deviceConnected = false;

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override {
    deviceConnected = true;
  }
  void onDisconnect(BLEServer* pServer) override {
    deviceConnected = false;
    // Restart advertising to allow companion reconnect
    pServer->startAdvertising();
  }
};

inline void init() {
  BLEDevice::init("WhereDaFlock-Dongle");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService* pService = pServer->createService(WDF_BLE_SERVICE_UUID);
  pTxCharacteristic = pService->createCharacteristic(
    WDF_BLE_CHARACTERISTIC_TX,
    BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
  );
  pTxCharacteristic->addDescriptor(new BLE2902());

  pService->start();
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(WDF_BLE_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
}

inline bool isConnected() {
  return deviceConnected;
}

inline void broadcast(const char* json) {
  if (deviceConnected && pTxCharacteristic != nullptr && json != nullptr) {
    pTxCharacteristic->setValue((uint8_t*)json, strlen(json));
    pTxCharacteristic->notify();
  }
}

} // namespace WhereDaFlockBLETelemetry

#else

namespace WhereDaFlockBLETelemetry {
inline void init() {}
inline bool isConnected() { return false; }
inline void broadcast(const char* /*json*/) {}
} // namespace WhereDaFlockBLETelemetry

#endif
