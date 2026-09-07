/*
 * WhereDaFlock Emulator - Flock Camera Signal Replicator (ESP32)
 * ==============================================================
 * A TEST-ONLY fixture that replicates the radio emissions Flock Safety
 * cameras produce, so you can validate the WhereDaFlock detector END-TO-END
 * on your bench WITHOUT a real camera nearby.
 *
 * It emits BOTH families of Flock signal, mirroring firmware/src/signatures.h
 * and firmware/src/ble_signatures.h:
 *
 *   1. CURRENT signal - 802.11 wildcard probe requests over 2.4GHz WiFi.
 *      Flock cameras today transmit wildcard probe requests (~0.125s interval,
 *      ascending channel hop). This sketch injects those frames with a real
 *      Flock OUI in the transmitter MAC, a wildcard SSID, and optionally a
 *      vendor-specific IE (tag 0xFF) to trigger the detector's top tier (4).
 *
 *   2. LEGACY signal - BLE beacon advertising the Flock Company Identifier
 *      0x09C8 (the signal defined in ble_signatures.h / old flock-* research).
 *
 * ⚠️  THIS SKETCH TRANSMITS ON 2.4GHz. It impersonates a Flock device purely
 * for lab self-testing of your own receiver on your own bench / own network.
 * Do NOT run it in public or near people who did not consent. Keep test runs
 * short. Remove/never deploy this file on a production device.
 *
 * ---------------------------------------------------------------------------
 * HOW TO USE (over-the-air test, TWO boards, YOUR PC WiFi IS NEVER TOUCHED)
 * ---------------------------------------------------------------------------
 *   Board A = WhereDaFlock DETECTOR (your M5StickC Plus on /dev/ttyUSB0,
 *            already running WhereDaFlock_scanner.cpp, sniffing 2.4GHz).
 *   Board B = this EMITTER (ANY second ESP32 - classic, S2, S3, C3...).
 *
 *   1. Flash Board B with this sketch. It does NOT join your network and
 *      does NOT use your PC's radio at all.
 *   2. Place Board B 1-3 meters from Board A (both on the bench, batteries
 *      or separate USB power).
 *   3. Power Board B. Watch Board A's serial output: it should print
 *      detection JSON like:
 *        {"event":"detection","detection_method":"wifi_wildcard_probe_ie_sig",
 *         "detection_tier":4,"protocol":"wifi_2_4ghz","mac_address":"82:6b:f2:...",
 *         "rssi":..,"channel":N,"frequency":..,"ssid":""}
 *
 * Build (PlatformIO):
 *   cd tools/emulator && pio run -e esp32dev            # or -e xiao_esp32s3
 *   pio run -e esp32dev -t upload --upload-port /dev/ttyUSB1
 *
 * Build (Arduino CLI):
 *   arduino-cli compile --fqbn esp32:esp32:esp32 FlockCam_emulator.ino
 *   arduino-cli upload --port /dev/ttyUSB1 --fqbn esp32:esp32:esp32 FlockCam_emulator.ino
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLEAdvertising.h>

// ---------------------------------------------------------------------------
// Tunables
// ---------------------------------------------------------------------------
#define NUM_CAMS        1          // how many "virtual cameras" to simulate
#define TIER            4          // 2=OUI frame, 3=wildcard probe, 4=+vendor IE
#define HOP_MS          125        // dwell per channel, matches real ~125ms cam hop
#define HOP_ASCENDING_PREFERRED 1  // cameras hop channels ASCENDING (1->11)
#define ENABLE_BLE_BEACON      1   // legacy Flock BLE 0x09C8 beacon
#define HOLD_LISTEN_SEC        60  // seconds the detector is likely watching

// ---------------------------------------------------------------------------
// Signatures (identical to firmware/src/signatures.h)
// ---------------------------------------------------------------------------
const char* FLOCK_OUIS[] = {
  "70:c9:4e","3c:91:80","d8:f3:bc","80:30:49","b8:35:32",
  "14:5a:fc","74:4c:a1","08:3a:88","9c:2f:9d","c0:35:32",
  "94:08:53","e4:aa:ea","f4:6a:dd","e0:0a:f6","24:b2:b9",
  "00:f4:8d","d0:39:57","e8:d0:fc","e0:4f:43","b8:1e:a4",
  "70:08:94","58:8e:81","ec:1b:bd","3c:71:bf","58:00:e3",
  "90:35:ea","5c:93:a2","64:6e:69","48:27:ea","a4:cf:12",
  "14:b5:cd","82:6b:f2"
};
const int FLOCK_OUI_COUNT = sizeof(FLOCK_OUIS) / sizeof(FLOCK_OUIS[0]);

// The 2.4GHz channels, ascending like real camera probe hops.
const uint8_t CHANNELS[] = {1,2,3,4,5,6,7,8,9,10,11};
const int CHANNEL_COUNT = sizeof(CHANNELS) / sizeof(CHANNELS[0]);

#define FLOCK_BLE_MFR_ID 0x09C8   // little-endian bytes 0xC8 0x09

// ---------------------------------------------------------------------------
// Raw 802.11 frame builder (mirrors flock_sim.py build_probe)
// ---------------------------------------------------------------------------

// Parse "70:c9:4e" style OUI into 3 raw bytes.
static void parse_oui(const char* o, uint8_t out[3]) {
  for (int i = 0; i < 3; i++) {
    out[i] = (uint8_t)strtol(o + i * 3, NULL, 16);
  }
}

// Build a random unicast MAC whose OUI is a known Flock OUI.
static void random_flock_mac(uint8_t mac[6]) {
  const char* oui = FLOCK_OUIS[esp_random() % FLOCK_OUI_COUNT];
  parse_oui(oui, mac);
  // Randomize the low 3 bytes. Avoid multicast bits in byte[0] only matters for
  // broadcast (0xff); the detector keeps locally-administered MACs, so byte[0]
  // bit 1 set (like 82:6b:f2) is fine and expected.
  for (int i = 3; i < 6; i++) mac[i] = (uint8_t)esp_random();
}

// Build a probe-request frame (without radiotap - esp_wifi_80211_tx expects
// the bare 802.11 frame). Returns byte length written to buf.
//   fc      = 0x0040  (type 0 management, subtype 4 probe request)
//   wildcard SSID IE (tag 0, len 0)
//   supported rates IE
//   DS channel IE
//   optional vendor IE (tag 0xFF) for tier 4
static size_t build_probe_frame(uint8_t buf[], const uint8_t ta[6],
                                uint16_t seq, uint8_t channel) {
  uint8_t* p = buf;
  uint16_t fc = 0x0040;                 // probe request
  uint16_t dur = 0;

  p[0] = fc & 0xFF; p[1] = (fc >> 8) & 0xFF; p += 2;
  p[0] = dur & 0xFF; p[1] = (dur >> 8) & 0xFF; p += 2;
  for (int i = 0; i < 6; i++) p[i] = 0xFF; p += 6;   // addr1 broadcast
  for (int i = 0; i < 6; i++) p[i] = ta[i];   p += 6; // addr2 = camera MAC
  for (int i = 0; i < 6; i++) p[i] = 0xFF; p += 6;   // addr3 broadcast
  p[0] = seq & 0xFF; p[1] = (seq >> 8) & 0xFF; p += 2; // seq ctrl

  // SSID IE: tag 0, length 0 == WILDCARD probe. This is the key Flock signal.
  p[0] = 0x00; p[1] = 0x00; p += 2;

  // Supported rates IE (matches flock_sim.py)
  static const uint8_t rates[] = {0x82,0x84,0x8B,0x96,0x0C,0x12,0x18,0x24};
  p[0] = 0x01; p[1] = sizeof(rates);
  memcpy(p + 2, rates, sizeof(rates)); p += 2 + sizeof(rates);

  // DS channel IE
  p[0] = 0x03; p[1] = 0x01; p[2] = channel; p += 3;

#if TIER >= 4
  // Vendor-specific IE (tag 0xFF) -> top-confidence fingerprint.
  static const uint8_t vendor[] = {0x00,0x01,0x02,0x03,0x04,0x05};
  p[0] = 0xFF; p[1] = sizeof(vendor);
  memcpy(p + 2, vendor, sizeof(vendor)); p += 2 + sizeof(vendor);
#endif

  return (size_t)(p - buf);
}

// ---------------------------------------------------------------------------
// ESP32 raw 802.11 injection via esp_wifi_80211_tx
// ---------------------------------------------------------------------------
// The ESP32 WiFi stack permits sending a crafted frame directly with
// esp_wifi_80211_tx(). We put the radio in promiscuous (receive) mode which is
// the state that accepts tx of arbitrary mgmt frames on the current channel.
static esp_err_t inject_frame(const uint8_t* frame, size_t len, uint8_t channel) {
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  return esp_wifi_80211_tx(WIFI_IF_STA, frame, len, false);
}

// ---------------------------------------------------------------------------
// BLE legacy beacon
// ---------------------------------------------------------------------------
static void start_ble_beacon() {
#if ENABLE_BLE_BEACON
  BLEDevice::init("FS Ext Battery");
  BLEAdvertising* adv = BLEDevice::getAdvertising();
  BLEAdvertisementData data;
  data.setName("FS Ext Battery");
  // Manufacturer data: little-endian Flock Company ID 0x09C8 then a fake blob.
  uint8_t mfr[] = {0xC8, 0x09, 0x01, 0x02, 0x03};
  data.setManufacturerData(std::string((char*)mfr, sizeof(mfr)));
  // Battery Service (supporting signal). Spell the UUID via uint16_t to avoid
  // the 16/32-bit BLEUUID overload ambiguity.
  BLEUUID svc((uint16_t)0x180F);
  data.setCompleteServices(svc);
  adv->setAdvertisementData(data);
  BLEAdvertisementData scanResp;
  adv->setScanResponseData(scanResp);
  BLEDevice::startAdvertising();
  Serial.println("[EMU] BLE beacon advertising Flock ID 0x09C8");
#endif
}

// ---------------------------------------------------------------------------
// Arduino
// ---------------------------------------------------------------------------
static uint8_t camMacs[NUM_CAMS][6];

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("WhereDaFlock EMITTER v2 - Flock camera signal replicator");
  Serial.println("TRANSMITTING on 2.4GHz. Bench use only. Detector must be listening.");
  Serial.printf("  virtual cams: %d | tier %d | hop %d ms | channels 1-11\n",
                NUM_CAMS, (int)TIER, (int)HOP_MS);

  // Random Flock-OUI MACs, one per virtual camera.
  for (int i = 0; i < NUM_CAMS; i++) {
    random_flock_mac(camMacs[i]);
    Serial.printf("  cam %d MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
                  i + 1, camMacs[i][0], camMacs[i][1], camMacs[i][2],
                  camMacs[i][3], camMacs[i][4], camMacs[i][5]);
  }

  // Radio setup for raw tx. We use STA mode + promiscuous so esp_wifi_80211_tx
  // can send arbitrary 802.11 management frames. We NEVER associate to a WiFi
  // network. (This emitter has its own radio; it does not use your PC's wifi.)
  WiFi.mode(WIFI_MODE_STA);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(CHANNELS[0], WIFI_SECOND_CHAN_NONE);
  delay(100);

#if ENABLE_BLE_BEACON
  start_ble_beacon();
#endif

  Serial.println("[EMU] emitting probe requests. Ctrl-C / unplug to stop.");
}

void loop() {
  static unsigned long lastHop = 0;
  static int chIdx = 0;
  static uint16_t seq = 0x100;

  // Hop channels ascending (1 -> 11) at ~125 ms, like a real Flock camera.
  if (millis() - lastHop >= HOP_MS) {
    lastHop = millis();
    chIdx = (chIdx + 1) % CHANNEL_COUNT;
  }
  uint8_t ch = CHANNELS[chIdx];

  for (int i = 0; i < NUM_CAMS; i++) {
    uint8_t frame[96];
    size_t len = build_probe_frame(frame, camMacs[i], seq, ch);
    esp_err_t err = inject_frame(frame, len, ch);
    seq = (seq + 0x09) & 0x0FFF;
    (void)err;
  }

  // Keep the BLE beacon advertised (the stack sometimes pauses it).
  static unsigned long lastAdv = 0;
#if ENABLE_BLE_BEACON
  if (millis() - lastAdv >= 5000) {
    lastAdv = millis();
    BLEDevice::startAdvertising();
  }
#endif

  // Pace frames. The detector hops 11->6->1 in reverse, so a modest rate on
  // each ascending channel is plenty for it to catch a probe.
  delay(HOP_MS);
}