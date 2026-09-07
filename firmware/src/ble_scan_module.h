// WhereDaFlock - BLE scan module (additive, time-sliced)
// ========================================================
// Adds Flock BLE beacon scanning to the WiFi promiscuous detector WITHOUT
// replacing or removing any WiFi logic. Because the ESP32 has a single 2.4 GHz
// radio, WiFi promiscuous sniffing and BLE scanning cannot run at the very same
// instant; this module runs a TIME-SLICED loop: the existing WiFi sniffer runs
// for a window, then BLE scans for a window, alternating forever.
//
// When WDF_ENABLE_BLE is 0 (the default), every function here compiles to a
// no-op and the firmware behaves exactly as today. Enable it by adding
//   -D WDF_ENABLE_BLE=1
// to a build env and including this header (see WhereDaFlock_scanner.cpp).
//
// RECEIVE-ONLY. This module never advertises and never connects; it only
// scans advertisements for the Flock Company Identifier 0x09C8 plus the
// supporting name / service-UUID signals (src/ble_signatures.h).

#ifndef WDF_ENABLE_BLE
#define WDF_ENABLE_BLE 0
#endif

#if WDF_ENABLE_BLE
#include <esp_wifi.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include "ble_signatures.h"
#include "display_m5stick.h"
#include "display_dongle.h"

extern int wdfDetCount;
void tierChirp(uint8_t tier);

namespace WhereDaFlockBLE {

// Windows (ms). WiFi + BLE alternate; each uses the radio exclusively.
constexpr unsigned long WIFI_WINDOW_MS = 8000;   // promiscuous WiFi sniff
constexpr unsigned long BLE_WINDOW_MS  = 4000;   // BLE scan
constexpr int BLE_SCAN_SECONDS = BLE_WINDOW_MS / 1000;   // per-window scan

constexpr int MAX_DEVICES  = 20;
constexpr int RSSI_FLOOR   = -90;
constexpr int TX_POWER_1M  = -59;

BLEScan* pBLEScan = nullptr;
bool bleInitDone = false;

struct Slot {
  String mac, name, method;
  int rssi = -127, confidence = 0;
  unsigned long lastSeenAt = 0;
  bool reported = false;
};
Slot slots[MAX_DEVICES];
int slotCount = 0;

struct BleAlertMailbox {
  char protocol[12];
  char name[32];
  char mac[24];
  char vendor[32];
  char method[24];
  char verdict[24];
  uint8_t category;
  int8_t rssi;
  float distM;
  uint8_t confidence;
  uint16_t hits;
  bool isNew;
  volatile bool pending;
};
static BleAlertMailbox alertMailbox = {};

// RSSI -> meters (same reference as the iOS app).
float bleDistance(int rssi) {
  if (rssi == 0) return -1.0f;
  float ratio = (float)rssi / (float)TX_POWER_1M;
  if (ratio < 1.0f) return powf(ratio, 10.0f);
  return (0.89976f * powf(ratio, 7.7095f)) + 0.111f;
}

static inline uint16_t bleGetMfrId(BLEAdvertisedDevice* d) {
  if (!d->haveManufacturerData()) return 0;
  std::string data = d->getManufacturerData();
  if (data.length() < 2) return 0;
  return (uint8_t)data[0] | ((uint8_t)data[1] << 8);
}

static inline bool bleHasServiceUuidFragment(BLEAdvertisedDevice* d, const char* targetFrag) {
  String target = toUpper(String(targetFrag));
  for (int i = 0; i < d->getServiceUUIDCount(); i++) {
    String up = toUpper(String(d->getServiceUUID(i).toString().c_str()));
    if (up.indexOf(target) >= 0) return true;
  }
  return false;
}

static inline bool bleMatchesAnyName(const String& name, const char* const* patterns, size_t count) {
  if (!name.length()) return false;
  String up = toUpper(name);
  for (size_t i = 0; i < count; i++) {
    String pat = toUpper(String(patterns[i]));
    if (up.indexOf(pat) >= 0) return true;
  }
  return false;
}

bool bleNameMatch(const String& name, String& which) {
  if (!name.length()) return false;
  String up = toUpper(name);
  for (size_t i = 0; i < NAME_PATTERN_COUNT; i++) {
    String pat = toUpper(String(NAME_PATTERNS[i]));
    if (up.indexOf(pat) >= 0) { which = NAME_PATTERNS[i]; return true; }
  }
  return false;
}

bool bleHasMfrId(BLEAdvertisedDevice* d) {
  return bleGetMfrId(d) == FLOCK_MFR_ID;
}

bool bleSvcMatch(BLEAdvertisedDevice* d, String& which) {
  for (int i = 0; i < d->getServiceUUIDCount(); i++) {
    String up = toUpper(String(d->getServiceUUID(i).toString().c_str()));
    for (size_t s = 0; s < SERVICE_UUID_FRAGMENT_COUNT; s++) {
      String frag = toUpper(String(SERVICE_UUID_FRAGMENTS[s]));
      if (up.indexOf(frag) >= 0) { which = SERVICE_UUID_FRAGMENTS[s]; return true; }
    }
  }
  return false;
}

int bleAlloc(const char* mac) {
  for (int i = 0; i < slotCount; i++) if (slots[i].mac == mac) return i;
  if (slotCount < MAX_DEVICES) {
    int i = slotCount++;
    slots[i].mac = mac; slots[i].lastSeenAt = 0;
    slots[i].confidence = 0; slots[i].rssi = -127; slots[i].reported = false;
    return i;
  }
  int oldest = 0;
  for (int i = 1; i < slotCount; i++)
    if (slots[i].lastSeenAt < slots[oldest].lastSeenAt) oldest = i;
  slots[oldest].mac = mac; slots[oldest].lastSeenAt = 0;
  slots[oldest].confidence = 0; slots[oldest].reported = false;
  return oldest;
}

class BleScanCb : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice d) override {
    std::string raw = d.getAddress().toString();
    const char* mac = raw.c_str();
    String name = String(d.getName().c_str());
    int rssi = d.getRSSI();
    if (rssi < RSSI_FLOOR) return;

    uint16_t mfrId = bleGetMfrId(&d);
    uint8_t category = CAT_UNKNOWN;
    const char* vendor = "Flock / BLE";
    const char* defaultName = "FS Ext Battery";
    const char* verdict = "CANDIDATE";
    int score = 0;
    String method = "none";

    // 1. Flock Safety check
    if (mfrId == FLOCK_MFR_ID) {
      category = CAT_FLOCK_ALPR;
      vendor = "Flock Safety (0x09C8)";
      defaultName = "FS Ext Battery";
      verdict = "FLOCK_LIKELY";
      score += MFR_ID_WEIGHT;
      method = "mfr_id";
    } else if (bleNameMatch(name, method)) {
      category = CAT_FLOCK_ALPR;
      vendor = "Flock Safety";
      defaultName = "Flock Camera";
      verdict = "FLOCK_LIKELY";
      score += NAME_MATCH_WEIGHT;
      method = "name";
    }

    // 2. Axon Enterprise Police Bodycam / Signal check
    if (mfrId == AXON_MFR_ID) {
      category = CAT_POLICE_BODYCAM;
      vendor = "Axon Enterprise";
      defaultName = "Axon Signal Beacon";
      verdict = "POLICE_BODYCAM";
      score = max(score, 85);
      method = "axon_mfr";
    }
    for (size_t s = 0; s < AXON_SIGNAL_UUID_COUNT; s++) {
      if (bleHasServiceUuidFragment(&d, AXON_SIGNAL_UUIDS[s])) {
        category = CAT_POLICE_BODYCAM;
        vendor = "Axon Enterprise";
        defaultName = "Axon Signal Sync";
        verdict = "POLICE_BODYCAM";
        score = max(score, 90);
        method = "axon_signal_uuid";
        break;
      }
    }
    if (bleMatchesAnyName(name, POLICE_NAME_PATTERNS, POLICE_NAME_PATTERN_COUNT)) {
      if (category == CAT_UNKNOWN) {
        category = CAT_POLICE_BODYCAM;
        vendor = "Axon Enterprise";
        defaultName = "Axon Police Device";
        verdict = "POLICE_BODYCAM";
      }
      score = max(score, 75);
      if (method == "none") method = "police_name";
    }

    // 3. Drone Remote ID check (ASTM F3411)
    if (bleHasServiceUuidFragment(&d, DRONE_RID_UUID)) {
      category = CAT_DRONE_UAV;
      vendor = "Drone Remote ID";
      defaultName = "FAA Drone RID";
      verdict = "DRONE_DETECTED";
      score = max(score, 90);
      method = "drone_rid_uuid";
    } else if (bleMatchesAnyName(name, DRONE_NAME_PATTERNS, DRONE_NAME_PATTERN_COUNT)) {
      if (category == CAT_UNKNOWN) {
        category = CAT_DRONE_UAV;
        vendor = "Drone Remote ID";
        defaultName = "UAV Drone Beacon";
        verdict = "DRONE_DETECTED";
      }
      score = max(score, 70);
      if (method == "none") method = "drone_name";
    }

    // Supporting secondary service UUIDs
    String whichSvc;
    if (bleSvcMatch(&d, whichSvc)) {
      score += SERVICE_UUID_WEIGHT;
      if (method == "none") {
        method = "service_uuid";
        category = CAT_FLOCK_ALPR;
        vendor = "Flock Safety";
        defaultName = "Flock Peripheral";
      }
    }

    if (score > 0 && rssi >= -50) score += RSSI_PROXIMITY_BONUS;
    if (score == 0) return;
    score = min(100, score);

    int idx = bleAlloc(mac);
    Slot& s = slots[idx];
    bool isNew = (s.lastSeenAt == 0);
    s.rssi = rssi; s.name = name; s.confidence = score;
    s.method = method; s.lastSeenAt = millis();
    bool isLikely = (score >= LIKELY_THRESHOLD);

    const char* finalName = name.length() ? name.c_str() : defaultName;

    // Rate-limit serial emission: immediate on isNew, otherwise at most once per second
    static unsigned long lastSerialEmit = 0;
    unsigned long now = millis();
    if (isNew || (now - lastSerialEmit >= 1000)) {
      lastSerialEmit = now;
      Serial.printf("{\"event\":\"%s\",\"protocol\":\"ble\",\"category\":\"%s\",\"mac\":\"%s\","
                    "\"name\":\"%s\",\"vendor\":\"%s\",\"rssi\":%d,\"dist_m\":%.2f,\"conf\":%d,"
                    "\"method\":\"%s\",\"verdict\":\"%s\"}\n",
                    isNew ? "new" : "update", categoryToString((TargetCategory)category), mac,
                    finalName, vendor,
                    rssi, bleDistance(rssi), score, method.c_str(), verdict);
    }

    if (isLikely || isNew) s.reported = true;

    if (isLikely) {
      static uint16_t bleHitCounter = 0;
      bleHitCounter++;

      // Safely handoff alert telemetry to loop() on the main thread
      if (!alertMailbox.pending) {
        strncpy(alertMailbox.protocol, "BLE 4.2", sizeof(alertMailbox.protocol) - 1);
        strncpy(alertMailbox.name, finalName, sizeof(alertMailbox.name) - 1);
        strncpy(alertMailbox.mac, mac, sizeof(alertMailbox.mac) - 1);
        strncpy(alertMailbox.vendor, vendor, sizeof(alertMailbox.vendor) - 1);
        strncpy(alertMailbox.method, method.c_str(), sizeof(alertMailbox.method) - 1);
        strncpy(alertMailbox.verdict, verdict, sizeof(alertMailbox.verdict) - 1);
        alertMailbox.category = category;
        alertMailbox.rssi = rssi;
        alertMailbox.distM = bleDistance(rssi);
        alertMailbox.confidence = score;
        alertMailbox.hits = bleHitCounter;
        alertMailbox.isNew = isNew;
        alertMailbox.pending = true;
      } else {
        alertMailbox.rssi = rssi;
        alertMailbox.distM = bleDistance(rssi);
        alertMailbox.hits = bleHitCounter;
      }
    }
  }
};

static void bleScanDoneCb(BLEScanResults results) {
  // BLE scan duration complete
  if (pBLEScan) {
    pBLEScan->clearResults();
  }
}

// Time-slicing state machine.
unsigned long lastSwitchAt = 0;
bool inBleWindow = false;

// Called from setup().
inline void bleSetup() {
  Serial.println("WhereDaFlock BLE scan module enabled (time-sliced).");
  BLEDevice::init("WDF");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new BleScanCb(), true);
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  bleInitDone = true;
  lastSwitchAt = millis();
  inBleWindow = false;                    // start in the WiFi window
}

// Called every loop() tick. Returns true while the BLE scanner currently owns
// the radio, in which case the caller should skip its WiFi work for this tick.
inline bool blePoll() {
  unsigned long now = millis();
  unsigned long period = WIFI_WINDOW_MS + BLE_WINDOW_MS;
  if (now - lastSwitchAt >= period) lastSwitchAt = now;   // wrap guard
  unsigned long elapsed = (now - lastSwitchAt) % period;
  bool wantBle = (elapsed >= WIFI_WINDOW_MS);

  if (wantBle && !inBleWindow) {
    // Entering the BLE window: pause WiFi promiscuous sniffing, start asynchronous BLE scan
    inBleWindow = true;
    esp_wifi_set_promiscuous(false);
    Serial.printf("[wdf] BLE scan window %us\n", BLE_SCAN_SECONDS);
    if (pBLEScan) {
      // Pass bleScanDoneCb so start() is ASYNCHRONOUS and never blocks loop()!
      pBLEScan->start(BLE_SCAN_SECONDS, bleScanDoneCb, false);
    }
  }
  if (!wantBle && inBleWindow) {
    // Exiting the BLE window back to WiFi.
    inBleWindow = false;
    if (pBLEScan) {
      pBLEScan->stop();
      pBLEScan->clearResults();
    }
    esp_wifi_set_promiscuous(true);
    Serial.println("[wdf] BLE window over, returning to WiFi");
  }

  return inBleWindow;
}

// Called from main loop() context to safely process BLE alerts on the main thread
inline void checkAlerts() {
  if (alertMailbox.pending) {
    bool isNew = alertMailbox.isNew;
    int8_t rssi = alertMailbox.rssi;
    float dist = alertMailbox.distM;
    uint16_t hits = alertMailbox.hits;
    alertMailbox.pending = false;

    bool chirpWorthy = false;
    wdfAddBleDetection(alertMailbox.mac, alertMailbox.name, alertMailbox.vendor,
                       alertMailbox.method, alertMailbox.verdict, alertMailbox.category,
                       rssi, dist, alertMailbox.confidence, &chirpWorthy);

    if (isNew) {
      if (alertMailbox.category == CAT_POLICE_VEHICLE || alertMailbox.category == CAT_POLICE_BODYCAM) {
        policeChirp();
      } else {
        ::tierChirp(4);
      }
#ifdef USE_M5STICKC_PLUS_DISPLAY
      m5stickDisplayShowAlertRich(alertMailbox.protocol, alertMailbox.name,
                                 alertMailbox.mac, alertMailbox.vendor,
                                 alertMailbox.method, alertMailbox.verdict,
                                 rssi, dist, alertMailbox.confidence,
                                 0, 6000, alertMailbox.category);
#else
      dongleDisplayShowAlert("BLE_TARGET", alertMailbox.mac, rssi, 0, 6000);
#endif
    } else {
#ifdef USE_M5STICKC_PLUS_DISPLAY
      m5stickDisplayUpdateAlertLive(rssi, dist, hits);
#endif
    }
  }
}

} // namespace WhereDaFlockBLE

#else // WDF_ENABLE_BLE == 0 -> no-op

namespace WhereDaFlockBLE {
inline void bleSetup() {}
inline bool blePoll() { return false; }
inline void checkAlerts() {}
}

#endif // WDF_ENABLE_BLE