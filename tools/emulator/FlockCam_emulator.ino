/*
 * WhereDaFlock Emulator - Flock Cam BLE Test Beacon (ESP32)
 * -------------------------------------------------------------
 * A TEST-ONLY fixture that advertises with the Flock Safety manufacturer
 * Company Identifier 0x09C8 so you can validate a WhereDaFlock BLE detector
 * without a real Flock camera nearby.
 *
 * ⚠️  THIS SKETCH TRANSMITS. It is a development/test beacon for validating
 * your own receiver on your own bench. Do NOT run it in public or near others;
 * it impersonates the Flock manufacturer ID purely for lab self-testing.
 * Remove/never deploy this file on a production detector.
 *
 * How to use:
 *   1. Flash this onto a separate ESP32 (your "test camera").
 *   2. Power it on a couple meters from a WhereDaFlock BLE detector.
 *   3. The detector should report a detection with method "mfr_id".
 *
 * Build (Arduino CLI):
 *   arduino-cli compile --fqbn esp32:esp32:esp32 FlockCam_emulator.ino
 */

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define FLOCK_MFR_ID 0x09C8   // little-endian bytes will be 0xC8 0x09

BLEServer* pServer = nullptr;
bool deviceConnected = false;

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*) { deviceConnected = true; }
  void onDisconnect(BLEServer*) { deviceConnected = false; }
};

void setup() {
  Serial.begin(115200);
  Serial.println("[EMU] Flock Cam BLE test beacon starting...");

  BLEDevice::init("FS Ext Battery");          // advertise a Flock-like name
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  // Advertise the Flock Company Identifier 0x09C8 as manufacturer data.
  // The first 2 bytes of manufacturer-specific data are the little-endian
  // Company Identifier, followed by an arbitrary payload the real camera
  // would carry (here a small status blob).
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  uint8_t mfrData[] = {
    0xC8, 0x09,             // little-endian Flock Company ID == 0x09C8
    0x01, 0x02, 0x03        // fake status payload (battery/temp bytes)
  };
  BLEAdvertisementData adv;
  adv.setName("FS Ext Battery");
  adv.setManufacturerData(std::string((char*)mfrData, sizeof(mfrData)));
  adv.setCompleteServices(BLEUUID("180F"));  // Battery Service (supporting signal)

  BLEAdvertisementData emptyScanResponse;   // no scan-response payload
  pAdvertising->setAdvertisementData(adv);
  pAdvertising->setScanResponseData(emptyScanResponse);
  BLEDevice::startAdvertising();

  Serial.println("[EMU] advertising with Flock MFR ID 0x09C8");
}

void loop() {
  // Re-advertise periodically in case the stack stops.
  static unsigned long last = 0;
  if (millis() - last > 5000) {
    last = millis();
    BLEDevice::startAdvertising();
    Serial.println("[EMU] re-advertising...");
  }
  delay(100);
}