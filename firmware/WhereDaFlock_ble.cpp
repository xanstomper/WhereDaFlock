/*
 * WhereDaFlock BLE - Flock Cam BLE Beacon Scanner (ESP32)
 * -------------------------------------------------------------
 * Passively scans BLE advertisements for Flock Safety hardware. The decisive
 * signal is the Flock manufacturer Company Identifier 0x09C8 present in the
 * advertisement's manufacturer-specific data. Supporting signals (advertised
 * device name, service UUIDs) are combined into a confidence score.
 *
 * RECEIVE-ONLY. This firmware never advertises, never connects, and never
 * writes to any device. It only listens to broadcast advertisements.
 *
 * Build (Arduino CLI):
 *   arduino-cli compile --fqbn esp32:esp32:esp32 WhereDaFlock_ble.ino
 *
 * Wiring (defaults, override below):
 *   BUZZER_PIN  -> piezo buzzer
 *   LED_PIN     -> LED (active low for XIAO ESP32-S3)
 *   USB         -> JSON lines @ 115200
 *
 * Board: any ESP32 with BLE (ESP32, ESP32-S3, ESP32-C3). Arduino core.
 */

#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <ArduinoJson.h>
#include "src/ble_signatures.h"

using namespace WhereDaFlockBLE;

// ---------------------------------------------------------------------------
// CONFIG
// ---------------------------------------------------------------------------
#ifndef BUZZER_PIN
#define BUZZER_PIN       3
#endif
#ifndef LED_PIN
#define LED_PIN         21
#endif
#ifndef LED_ACTIVE_HIGH
#define LED_ACTIVE_HIGH  0       // XIAO onboard LED is active low
#endif

constexpr int   SCAN_DURATION      = 5;      // seconds per scan window
constexpr int   MAX_DEVICES        = 24;     // distinct devices kept
constexpr int   RSSI_THRESHOLD     = -90;    // ignore weak signals
constexpr int   TX_POWER_1M        = -59;    // reference RSSI at 1 m
constexpr long  HEARTBEAT_MS       = 10000;  // re-beep while in range
constexpr int   ALERT_MS           = 120;
constexpr int   ALERT_FREQ_HZ      = 2700;

BLEScan* pBLEScan = nullptr;

// ---------------------------------------------------------------------------
// Device slot
// ---------------------------------------------------------------------------
struct DeviceSlot {
  String   mac;
  String   name;
  int      rssi;
  int      confidence;
  String   method;                 // "mfr_id" | "name" | "service_uuid"
  unsigned long lastSeenAt;
  bool     reported;
};
DeviceSlot slots[MAX_DEVICES];
int slotCount = 0;
unsigned long lastHeartbeat = 0;

// ---------------------------------------------------------------------------
// RSSI -> distance estimate (same reference as the iOS companion app).
// ---------------------------------------------------------------------------
float estimateDistance(int rssi) {
  if (rssi == 0) return -1.0f;
  float ratio = (float)rssi / (float)TX_POWER_1M;
  if (ratio < 1.0f) return powf(ratio, 10.0f);
  return (0.89976f * powf(ratio, 7.7095f)) + 0.111f;
}

// ---------------------------------------------------------------------------
// Name matching (secondary signal).
// ---------------------------------------------------------------------------
bool matchesName(const String& name, String& which) {
  if (name.length() == 0) return false;
  String up = toUpper(name);
  for (size_t i = 0; i < NAME_PATTERN_COUNT; i++) {
    String pat = toUpper(String(NAME_PATTERNS[i]));
    if (up.indexOf(pat) >= 0) { which = NAME_PATTERNS[i]; return true; }
  }
  return false;
}

// ---------------------------------------------------------------------------
// Manufacturer data -> Flock Company ID check (little-endian first 2 bytes).
// This is the decisive Flock signal.
// ---------------------------------------------------------------------------
bool hasFlockMfrId(BLEAdvertisedDevice* device) {
  if (!device->haveManufacturerData()) return false;
  std::string data = device->getManufacturerData();
  if (data.length() < 2) return false;
  uint16_t mfrId = (uint8_t)data[0] | ((uint8_t)data[1] << 8);
  return (mfrId == FLOCK_MFR_ID);
}

// ---------------------------------------------------------------------------
// Service UUID matching (weak supporting signal).
// ---------------------------------------------------------------------------
bool matchesServiceUUID(BLEAdvertisedDevice* device, String& which) {
  for (int i = 0; i < device->getServiceUUIDCount(); i++) {
    String uuid = String(device->getServiceUUID(i).toString().c_str());
    String up = toUpper(uuid);
    for (size_t s = 0; s < SERVICE_UUID_FRAGMENT_COUNT; s++) {
      String frag = toUpper(String(SERVICE_UUID_FRAGMENTS[s]));
      if (up.indexOf(frag) >= 0) { which = SERVICE_UUID_FRAGMENTS[s]; return true; }
    }
  }
  return false;
}

// ---------------------------------------------------------------------------
// Alert buzz + LED flash.
// ---------------------------------------------------------------------------
void beep(int ms = ALERT_MS) {
  digitalWrite(LED_PIN, LED_ACTIVE_HIGH ? HIGH : LOW);
  tone(BUZZER_PIN, ALERT_FREQ_HZ);
  delay(ms);
  noTone(BUZZER_PIN);
  digitalWrite(LED_PIN, LED_ACTIVE_HIGH ? LOW : HIGH);
}

// ---------------------------------------------------------------------------
// Find / allocate a slot.
// ---------------------------------------------------------------------------
int findOrAllocateSlot(const char* mac) {
  for (int i = 0; i < slotCount; i++) if (slots[i].mac == mac) return i;
  if (slotCount < MAX_DEVICES) {
    int i = slotCount++;
    slots[i].mac = mac;
    slots[i].lastSeenAt = 0;
    slots[i].confidence = 0;
    slots[i].rssi = -127;
    slots[i].reported = false;
    return i;
  }
  int oldest = 0;
  for (int i = 1; i < slotCount; i++)
    if (slots[i].lastSeenAt < slots[oldest].lastSeenAt) oldest = i;
  slots[oldest].mac = mac;
  slots[oldest].lastSeenAt = 0;
  slots[oldest].confidence = 0;
  slots[oldest].reported = false;
  return oldest;
}

// ---------------------------------------------------------------------------
// Score + report an advertisement.
// ---------------------------------------------------------------------------
void handleDiscovery(BLEAdvertisedDevice* device) {
  std::string rawMac = device->getAddress().toString();
  const char* mac = rawMac.c_str();
  String name = String(device->getName().c_str());
  int rssi = device->getRSSI();
  if (rssi < RSSI_THRESHOLD) return;

  int score = 0;
  String method = "none";

  if (hasFlockMfrId(device)) {
    score += MFR_ID_WEIGHT;
    method = "mfr_id";
  }
  String which;
  if (name.length() > 0 && matchesName(name, which)) {
    score += NAME_MATCH_WEIGHT;
    if (method == "none") method = "name";
  }
  if (matchesServiceUUID(device, which)) {
    score += SERVICE_UUID_WEIGHT;
    if (method == "none") method = "service_uuid";
  }
  if (score > 0 && rssi >= -50) score += RSSI_PROXIMITY_BONUS;

  if (score == 0) return;                      // no Flock-related signal
  score = min(100, score);

  int idx = findOrAllocateSlot(mac);
  DeviceSlot& s = slots[idx];
  bool isNew = (s.lastSeenAt == 0);
  s.rssi = rssi;
  s.name = name;
  s.confidence = score;
  s.method = method;
  s.lastSeenAt = millis();

  bool isLikely = (score >= LIKELY_THRESHOLD);

  StaticJsonDocument<320> doc;
  doc["event"]   = isNew ? "new" : "update";
  doc["protocol"]= "ble";
  doc["mac"]     = mac;
  doc["name"]    = name.length() ? name : "?";
  doc["rssi"]    = rssi;
  doc["dist_m"]  = estimateDistance(rssi);
  doc["conf"]    = score;
  doc["method"]  = method;
  doc["mfr_id"]  = "0x09C8";
  doc["verdict"] = isLikely ? "FLOCK_LIKELY"
                 : (score >= POSSIBLE_THRESHOLD ? "FLOCK_POSSIBLE" : "CANDIDATE");
  serializeJson(doc, Serial);
  Serial.println();

  if (isLikely || isNew) {
    beep();
    s.reported = true;
  }
}

// ---------------------------------------------------------------------------
// Scanner callback
// ---------------------------------------------------------------------------
class ScanCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) override {
    handleDiscovery(&advertisedDevice);
  }
};

// ---------------------------------------------------------------------------
// Arduino
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LED_ACTIVE_HIGH ? LOW : HIGH);
  pinMode(BUZZER_PIN, OUTPUT);

  Serial.println("WhereDaFlock BLE v1.0.0 - passive Flock BLE beacon scanner");
  Serial.println("RECEIVE-ONLY. No transmissions, no connections.");
  Serial.println("Targeting Flock manufacturer ID 0x09C8 + name/UUID signals");

  BLEDevice::init("WhereDaFlock-BLE-RX");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new ScanCallbacks());
  pBLEScan->setActiveScan(true);     // request scan responses for more info
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  // Results are processed live via the callback; start() returns the set which
  // we discard each cycle.

  Serial.println("Scanning...");
}

void loop() {
  // start() returns BLEScanResults by value; the callback has already
  // processed matches live, so we just clear and move on.
  (void)pBLEScan->start(SCAN_DURATION, false);
  pBLEScan->clearResults();

  // Heartbeat while likely Flock devices remain in range.
  if (millis() - lastHeartbeat >= HEARTBEAT_MS) {
    lastHeartbeat = millis();
    for (int i = 0; i < slotCount; i++) {
      DeviceSlot& s = slots[i];
      if (s.confidence >= LIKELY_THRESHOLD && s.lastSeenAt > 0 &&
          millis() - s.lastSeenAt < HEARTBEAT_MS + 2000) {
        beep();
        break;
      }
    }
  }

  // start() returns BLEScanResults by value; nothing to free.
}