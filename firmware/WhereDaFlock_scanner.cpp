#include <Arduino.h>
/*
 * WhereDaFlock - Flock Cam 2.4GHz Passive Detector (ESP32)
 * -------------------------------------------------------------
 * Passively sniffs the 2.4GHz WiFi spectrum for 802.11 frames emitted by
 * Flock Safety ALPR / edge cameras. Flock cameras today transmit wildcard
 * probe requests on ascending channels; their transmitter MACs belong to a
 * known set of OUIs, and their probe frames carry a distinctive Information
 * Element fingerprint.
 *
 * This firmware is RECEIVE-ONLY (promiscuous mode). The radio never transmits,
 * never probes, and never associates to a network. No AP is created.
 *
 * Detection tiers (higher = more confident):
 *   4  wildcard_probe_ie_sig  OUI + wildcard SSID + IE fingerprint
 *   3  wildcard_probe         OUI + wildcard SSID (IE not verified)
 *   2  oui_addr2              transmitter-side OUI on any frame
 *   1  oui_addr1/addr3        receiver / BSSID OUI (AP echo, noisier)
 *   0  ssid                   SSID keyword (off by default)
 *
 * Wiring (Seeed XIAO ESP32-S3 defaults, override below):
 *   GPIO 3   -> piezo buzzer
 *   GPIO 21  -> onboard LED (active low)
 *   USB CDC  -> JSON detection lines @ 115200
 *
 * Build: PlatformIO or Arduino CLI (WiFi.promiscuous API).
 */

#include <WiFi.h>
#include <esp_wifi.h>
#include "src/signatures.h"
#include "src/session.h"
#include "src/hal.h"
#include "src/display_dongle.h"
#include "src/display_m5stick.h"
#include "src/ble_telemetry.h"
#include "src/ble_scan_module.h"   // additive, time-sliced BLE scanning (opt-in)

using namespace WhereDaFlock;

// ---------------------------------------------------------------------------
// CONFIG
// ---------------------------------------------------------------------------
#ifndef USE_BUZZER
#define USE_BUZZER        1
#endif
#ifndef BUZZER_PIN
#define BUZZER_PIN        3
#endif
#ifndef LED_PIN
#define LED_PIN          21
#endif
#ifndef LED_ACTIVE_HIGH
#define LED_ACTIVE_HIGH   0      // XIAO onboard LED is active-low
#endif

#define CHANNEL_DWELL_MS   250   // 2x observed 125ms camera hop time
#define RSSI_MIN           -95
#define ALERT_COOLDOWN_MS  5000
#define HEARTBEAT_MS       30000
#define HB_DEVICE_ACTIVE_MS 3000
#define HB_BEEP_INTERVAL_MS 10000
#define REDISCOVER_MS      30000
#define MAX_DETECTIONS     200
#define DEDUPE_SLOTS       8
#define ALERT_QUEUE_SIZE   32
#define BEEP_MASK_DEFAULT  0x1F  // all five tiers audible by default

// ---------------------------------------------------------------------------
// Channel hopping. "Descending" order matches the cameras' ascending hop for
// faster catch. Select a mode with CHANNEL_MODE:
//   0 = FULL_HOP  (11..1, descending)
//   1 = CUSTOM    (11/6/1 descending, default)
//   2 = SINGLE    (stay on SINGLE_CHANNEL)
// ---------------------------------------------------------------------------
#define CHANNEL_MODE_CUSTOM   1
#define CHANNEL_MODE_FULL_HOP 0
#define CHANNEL_MODE_SINGLE   2
#ifndef CHANNEL_MODE
#define CHANNEL_MODE CHANNEL_MODE_CUSTOM
#endif
#ifndef CHANNEL_DWELL_MS
#define CHANNEL_DWELL_MS 250    // 2x the observed 125ms camera hop
#endif
#ifndef SINGLE_CHANNEL
#define SINGLE_CHANNEL 1
#endif

static const uint8_t customChannels[] = {11, 6, 1};
static constexpr size_t customChannelCount = sizeof(customChannels)/sizeof(customChannels[0]);
static const uint8_t fullHopChannels[] = {11,10,9,8,7,6,5,4,3,2,1};
static constexpr size_t fullHopChannelCount = sizeof(fullHopChannels)/sizeof(fullHopChannels[0]);

static const uint8_t* activeChannels = customChannels;
static size_t         activeChannelCount = customChannelCount;

// Audio cadence per tier (distinct so you can identify by ear).
#define T4_LO_HZ 2000
#define T4_HI_HZ 2800
#define T3_LO_HZ 1400
#define T3_HI_HZ 1800
#define T2_HZ    1200
#define T1_HZ     800
#define T0_HZ     600
#define TIER_NOTE_MS 55
#define TIER_GAP_MS  25
#define BLIP_MS      45
#define HB_BEEP_HZ   1500
#define HB_NOTE_MS   70
#define HB_GAP_MS    70
#define LED_FLASH_MS 120

#define ENABLE_SSID_MATCH 0
#define CHECK_ADDR1 1
#define CHECK_ADDR3 1
static const char* target_ssid_keywords[] = { "flock" };

// ---------------------------------------------------------------------------
// 802.11 header
// ---------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
  uint16_t frame_ctrl;
  uint16_t duration;
  uint8_t  addr1[6];
  uint8_t  addr2[6];
  uint8_t  addr3[6];
  uint16_t seq_ctrl;
} wifi_ieee80211_mac_hdr_t;

// Pre-compiled OUI byte table (built once in setup so the matcher stays in IRAM).
static uint8_t all_oui_bytes[ALL_OUI_COUNT][3];

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static uint8_t        currentChannel = 1;
static size_t         chanIdx = 0;
static unsigned long  lastHop = 0;
static volatile bool  sniffingStopped = false;

// Alert ring buffer (ISR/callback -> loop), avoids Serial/malloc in the WiFi task.
typedef struct {
  uint8_t tier;
  uint8_t category;
  uint8_t mac[6];
  int8_t  rssi;
  uint8_t channel;
  bool    wildcardProbe;
  char    ssid[33];
  char    frameKind[12];
  char    name[24];
  char    vendor[24];
  char    verdict[20];
} AlertEntry;
static volatile AlertEntry alertQueue[ALERT_QUEUE_SIZE];
static volatile size_t alertHead = 0, alertTail = 0;
static portMUX_TYPE queueMux = portMUX_INITIALIZER_UNLOCKED;

// Detection table (unique by MAC, best tier retained).
// Defined here at GLOBAL scope with external linkage to match the extern
// declarations in src/session.h, so the SPIFFS persistence + host-command
// module operates on the same table.
WDFDetection wdfDet[WDF_MAX_DETECTIONS];
int          wdfDetCount = 0;

// Dedupe/serial-rate-limit table (suppresses beep+emit, allows tier upgrades).
static struct {
  char mac[18];
  unsigned long ts;
  uint8_t tier;
} dedupeTable[DEDUPE_SLOTS];
static size_t dedupeIdx = 0;

static volatile unsigned long ledOffAt = 0;
static unsigned long fyLastTargetSeen = 0, fyLastHeartbeatAt = 0;
static uint8_t fyLastTargetTier = 0;
// wdfBeepMask is defined here and declared extern in src/session.h (for the
// NVS-backed per-tier audio mute). Default: all five tiers audible.
volatile uint8_t wdfBeepMask = BEEP_MASK_DEFAULT;

// ---------------------------------------------------------------------------
// LED / buzzer helpers
// ---------------------------------------------------------------------------
static inline void ledSet(bool on) { wdf_hal::ledSet(on); }
static void ledTick() {
  if (ledOffAt && (long)(millis() - ledOffAt) >= 0) { wdf_hal::ledSet(false); ledOffAt = 0; }
}
static inline bool tierAudible(uint8_t tier) {
  return tier < TIER_COUNT && ((wdfBeepMask >> tier) & 0x01);
}
static void blip(uint16_t hz) { wdf_hal::toneStart(hz); delay(BLIP_MS); wdf_hal::toneStop(); }
static void chirp2(uint16_t lo, uint16_t hi) {
  wdf_hal::toneStart(lo); delay(TIER_NOTE_MS); wdf_hal::toneStop();
  delay(TIER_GAP_MS);
  wdf_hal::toneStart(hi); delay(TIER_NOTE_MS); wdf_hal::toneStop();
}
void tierChirp(uint8_t tier) {
  if (!tierAudible(tier)) return;
  switch (tier) {
    case TIER_IE_SIG: chirp2(T4_LO_HZ, T4_HI_HZ); break;
    case TIER_PROBE:  chirp2(T3_LO_HZ, T3_HI_HZ); break;
    case TIER_OUI:    blip(T2_HZ);                break;
    case TIER_ECHO:   blip(T1_HZ);                break;
    case TIER_SSID:   blip(T0_HZ);                break;
    default: break;
  }
}
void policeChirp() {
  wdf_hal::toneStart(950); delay(50); wdf_hal::toneStop();
  delay(15);
  wdf_hal::toneStart(1450); delay(75); wdf_hal::toneStop();
}
static void heartbeatBeep() {
  wdf_hal::toneStart(HB_BEEP_HZ); delay(HB_NOTE_MS); wdf_hal::toneStop();
  delay(HB_GAP_MS);
  wdf_hal::toneStart(HB_BEEP_HZ); delay(HB_NOTE_MS); wdf_hal::toneStop();
}

static void startupBeep() {
#if USE_BUZZER
  // First 6 notes of SMB World 1-2 (underground). Koji Kondo's descending
  // pattern: C4, C5, A3, A4, B♭3, B♭4 (alternating-octave pairs).
  static const uint16_t notes[6] = { 262, 523, 220, 440, 233, 466 };
  for (int i = 0; i < 6; i++) {
    wdf_hal::toneStart(notes[i]);
    delay((i == 5) ? 160 : 95);
    wdf_hal::toneStop();
    if (i < 5) delay(22);
  }
#endif
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static inline bool isMulticast(const uint8_t* mac) { return mac[0] & 0x01; }

static int IRAM_ATTR matchAllOuiRaw(const uint8_t* mac) {
  for (size_t i = 0; i < ALL_OUI_COUNT; i++) {
    if (mac[0] == all_oui_bytes[i][0] && mac[1] == all_oui_bytes[i][1] && mac[2] == all_oui_bytes[i][2])
      return (int)i;
  }
  return -1;
}

static inline bool IRAM_ATTR matchOuiRaw(const uint8_t* mac) {
  return matchAllOuiRaw(mac) >= 0;
}

static void macToStr(const uint8_t* mac, char* buf, size_t len) {
  snprintf(buf, len, "%02x:%02x:%02x:%02x:%02x:%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// Check the 802.11 header + trailer for a management probe request with a
// wildcard SSID (tag 0, length 0) and, on the top tier, the Flock IE
// fingerprint.
static bool IRAM_ATTR isWildcardProbe(const wifi_ieee80211_mac_hdr_t* hdr,
                                       const uint8_t* body, size_t bodyLen,
                                       bool* outHasFlockIe) {
  *outHasFlockIe = false;
  uint16_t fc = hdr->frame_ctrl;
  if ((fc & 0x000C) != 0x0000) return false;
  if (((fc >> 4) & 0x0F) != 0x04) return false;

  const uint8_t* p = body;
  const uint8_t* end = body + bodyLen;
  if (end - p < 2) return false;
  uint8_t tag = p[0], len = p[1];
  bool wildcard = (tag == 0 && len == 0);
  p += 2 + len;
  if (p > end) p = end;
  while (p + 2 <= end) {
    uint8_t t = p[0], l = p[1];
    if (p + 2 + l > end) break;
    if (t == 0xFF && l > 0) *outHasFlockIe = true;
    p += 2 + l;
  }
  return wildcard;
}

// ---------------------------------------------------------------------------
// Ring buffer helpers
// ---------------------------------------------------------------------------
static void IRAM_ATTR enqueueAlert(uint8_t tier, uint8_t category, const uint8_t* mac,
                                   int8_t rssi, uint8_t ch, bool wc, const char* ssid,
                                   const char* name, const char* vendor,
                                   const char* verdict, const char* kind) {
  portENTER_CRITICAL_ISR(&queueMux);
  size_t next = (alertHead + 1) % ALERT_QUEUE_SIZE;
  if (next == alertTail) { portEXIT_CRITICAL_ISR(&queueMux); return; } // full: drop
  AlertEntry* e = (AlertEntry*)&alertQueue[alertHead];
  e->tier = tier;
  e->category = category;
  e->rssi = rssi;
  e->channel = ch;
  e->wildcardProbe = wc;
  memcpy((void*)e->mac, mac, 6);
  if (ssid) strncpy((char*)e->ssid, ssid, 32); else e->ssid[0] = '\0';
  e->ssid[32] = '\0';
  if (name) strncpy((char*)e->name, name, 23); else e->name[0] = '\0';
  e->name[23] = '\0';
  if (vendor) strncpy((char*)e->vendor, vendor, 23); else e->vendor[0] = '\0';
  e->vendor[23] = '\0';
  if (verdict) strncpy((char*)e->verdict, verdict, 19); else e->verdict[0] = '\0';
  e->verdict[19] = '\0';
  if (kind) strncpy((char*)e->frameKind, kind, 11); else e->frameKind[0] = '\0';
  e->frameKind[11] = '\0';
  alertHead = next;
  portEXIT_CRITICAL_ISR(&queueMux);
}

// ---------------------------------------------------------------------------
// WiFi promiscuous callback (runs in the WiFi task - IRAM, no Serial, no malloc)
// ---------------------------------------------------------------------------
static void IRAM_ATTR wifiSniffer(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT && type != WIFI_PKT_DATA) return;
  wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  const wifi_ieee80211_mac_hdr_t* hdr =
      (const wifi_ieee80211_mac_hdr_t*)pkt->payload;

  // 1. Transmitter (addr2) multi-threat OUI match
  if (!isMulticast(hdr->addr2)) {
    int matchIdx = matchAllOuiRaw(hdr->addr2);
    if (matchIdx >= 0) {
      const OuiSignatureEntry& sig = ALL_TARGET_OUIS[matchIdx];
      uint8_t tier = (sig.category == CAT_FLOCK_ALPR) ? TIER_OUI : TIER_PROBE;
      bool wc = false, hasIe = false;
      if (type == WIFI_PKT_MGMT) {
        const uint8_t* body = pkt->payload + sizeof(wifi_ieee80211_mac_hdr_t);
        size_t bodyLen = pkt->rx_ctrl.sig_len > sizeof(wifi_ieee80211_mac_hdr_t)
                         ? pkt->rx_ctrl.sig_len - sizeof(wifi_ieee80211_mac_hdr_t) : 0;
        if (isWildcardProbe(hdr, body, bodyLen, &hasIe)) {
          wc = true;
          if (sig.category == CAT_FLOCK_ALPR) {
            tier = hasIe ? TIER_IE_SIG : TIER_PROBE;
          }
        }
      }
      enqueueAlert(tier, sig.category, hdr->addr2, (int8_t)pkt->rx_ctrl.rssi,
                   currentChannel, wc, "", sig.defaultName, sig.vendor, sig.verdict, "addr2");
      return;
    }
  }

  // 2. Management body analysis (Open Drone ID IE, Axon/Police/ALPR SSIDs)
  if (type == WIFI_PKT_MGMT) {
    const uint8_t* body = pkt->payload + sizeof(wifi_ieee80211_mac_hdr_t);
    size_t bodyLen = pkt->rx_ctrl.sig_len > sizeof(wifi_ieee80211_mac_hdr_t)
                     ? pkt->rx_ctrl.sig_len - sizeof(wifi_ieee80211_mac_hdr_t) : 0;

    // Check for Open Drone ID OUI (FA:0B:BC) in Information Elements
    if (bodyLen >= 6) {
      const uint8_t* p = body;
      const uint8_t* end = body + bodyLen;
      while (p + 2 <= end) {
        uint8_t t = p[0], l = p[1];
        if (p + 2 + l > end) break;
        if (t == 0xDD && l >= 3) {
          if (p[2] == 0xFA && p[3] == 0x0B && p[4] == 0xBC) {
            enqueueAlert(TIER_PROBE, CAT_DRONE_UAV, hdr->addr2, (int8_t)pkt->rx_ctrl.rssi,
                         currentChannel, false, "OpenDroneID", "FAA Drone Remote ID",
                         "OpenDroneID", "DRONE_DETECTED", "drone_ie");
            return;
          }
        }
        p += 2 + l;
      }
    }

    // Check SSID in Beacon / Probe Response / Probe Request
    if (bodyLen >= 2 && body[0] == 0) {
      uint8_t ssidLen = body[1];
      if (ssidLen > 0 && ssidLen <= 32 && (size_t)(ssidLen + 2) <= bodyLen) {
        char sBuf[33];
        memcpy(sBuf, body + 2, ssidLen);
        sBuf[ssidLen] = '\0';
        for (size_t k = 0; k < SSID_KEYWORD_COUNT; k++) {
          if (strcasestr(sBuf, TARGET_SSID_KEYWORDS[k].pattern)) {
            enqueueAlert(TIER_PROBE, TARGET_SSID_KEYWORDS[k].category, hdr->addr2,
                         (int8_t)pkt->rx_ctrl.rssi, currentChannel, false, sBuf,
                         TARGET_SSID_KEYWORDS[k].defaultName,
                         TARGET_SSID_KEYWORDS[k].vendor,
                         TARGET_SSID_KEYWORDS[k].verdict, "ssid");
            return;
          }
        }
      }
    }
  }

#if CHECK_ADDR1
  if (!isMulticast(hdr->addr1)) {
    int m1 = matchAllOuiRaw(hdr->addr1);
    if (m1 >= 0) {
      const OuiSignatureEntry& sig = ALL_TARGET_OUIS[m1];
      enqueueAlert(TIER_ECHO, sig.category, hdr->addr1, (int8_t)pkt->rx_ctrl.rssi,
                   currentChannel, false, "", sig.defaultName, sig.vendor, sig.verdict, "addr1");
    }
  }
#endif
#if CHECK_ADDR3
  int m3 = matchAllOuiRaw(hdr->addr3);
  if (m3 >= 0) {
    const OuiSignatureEntry& sig = ALL_TARGET_OUIS[m3];
    enqueueAlert(TIER_ECHO, sig.category, hdr->addr3, (int8_t)pkt->rx_ctrl.rssi,
                 currentChannel, false, "", sig.defaultName, sig.vendor, sig.verdict, "addr3");
  }
#endif
}

// ---------------------------------------------------------------------------
// Detection table
// ---------------------------------------------------------------------------
static bool shouldSuppressDuplicate(const char* macStr, uint8_t tier) {
  unsigned long now = millis();
  for (size_t i = 0; i < DEDUPE_SLOTS; i++) {
    if (strcmp(dedupeTable[i].mac, macStr) == 0) {
      bool cooling = (now - dedupeTable[i].ts) < ALERT_COOLDOWN_MS;
      bool upgrade = tier > dedupeTable[i].tier;
      if (cooling && !upgrade) return true;
      dedupeTable[i].ts = now;
      dedupeTable[i].tier = upgrade ? tier : (cooling ? dedupeTable[i].tier : tier);
      return false;
    }
  }
  strlcpy(dedupeTable[dedupeIdx].mac, macStr, 18);
  dedupeTable[dedupeIdx].ts = now;
  dedupeTable[dedupeIdx].tier = tier;
  dedupeIdx = (dedupeIdx + 1) % DEDUPE_SLOTS;
  return false;
}

int wdfAddGenericDetection(const char* mac, const char* name, const char* proto,
                           const char* vendor, const char* method, const char* verdict,
                           uint8_t category, uint8_t tier, int8_t rssi, uint8_t ch,
                           float distM, uint8_t conf, bool* outChirpWorthy) {
  uint32_t now = millis();
  for (int i = 0; i < wdfDetCount; i++) {
    if (strcmp(wdfDet[i].mac, mac) == 0) {
      bool rediscover = (now - wdfDet[i].lastSeen) > REDISCOVER_MS;
      if (wdfDet[i].count < 0xFFFF) wdfDet[i].count++;
      wdfDet[i].lastSeen = now;
      wdfDet[i].rssi = rssi;
      if (ch > 0) wdfDet[i].channel = ch;
      if (distM >= 0) wdfDet[i].distM = distM;
      if (conf > wdfDet[i].confidence) wdfDet[i].confidence = conf;
      bool upgrade = tier > wdfDet[i].tier;
      if (upgrade) {
        wdfDet[i].tier = tier;
        if (method && strlen(method)) strlcpy(wdfDet[i].method, method, sizeof(wdfDet[i].method));
        if (verdict && strlen(verdict)) strlcpy(wdfDet[i].verdict, verdict, sizeof(wdfDet[i].verdict));
      }
      if (name && strlen(name) && (strlen(wdfDet[i].name) == 0 || strcmp(wdfDet[i].name, "?") == 0)) {
        strlcpy(wdfDet[i].name, name, sizeof(wdfDet[i].name));
      }
      if (outChirpWorthy) *outChirpWorthy = rediscover || upgrade;
      return i;
    }
  }
  if (wdfDetCount >= MAX_DETECTIONS) {
    if (outChirpWorthy) *outChirpWorthy = false;
    return -1;
  }
  WDFDetection& d = wdfDet[wdfDetCount];
  strlcpy(d.mac, mac, sizeof(d.mac));
  strlcpy(d.name, (name && strlen(name)) ? name : "Unknown Target", sizeof(d.name));
  strlcpy(d.protocol, (proto && strlen(proto)) ? proto : "WiFi", sizeof(d.protocol));
  strlcpy(d.vendor, (vendor && strlen(vendor)) ? vendor : "Surveillance", sizeof(d.vendor));
  strlcpy(d.method, (method && strlen(method)) ? method : "OUI", sizeof(d.method));
  strlcpy(d.verdict, (verdict && strlen(verdict)) ? verdict : "SUSPECT", sizeof(d.verdict));
  d.category = category;
  d.tier = tier;
  d.rssi = rssi;
  d.distM = (distM >= 0) ? distM : ((rssi == 0) ? -1.0f : powf(10.0f, (-40.0f - (float)rssi) / 20.0f));
  d.confidence = conf;
  d.channel = ch;
  d.firstSeen = d.lastSeen = now;
  d.count = 1;
  d.ssid[0] = '\0';
  wdfDetCount++;
  if (outChirpWorthy) *outChirpWorthy = true;
  return wdfDetCount - 1;
}

int wdfAddBleDetection(const char* mac, const char* name, const char* vendor,
                       const char* method, const char* verdict, uint8_t category,
                       int8_t rssi, float distM, uint8_t conf, bool* outChirpWorthy) {
  return wdfAddGenericDetection(mac, name, "BLE", vendor, method, verdict,
                                category, 4, rssi, 0, distM, conf, outChirpWorthy);
}

static int fyAddDetection(const char* mac, const char* name, const char* vendor,
                          const char* method, const char* verdict, uint8_t category,
                          uint8_t tier, int8_t rssi, uint8_t ch, const char* ssid,
                          bool* outChirpWorthy) {
  const char* defName = (name && strlen(name)) ? name : ((ssid && strlen(ssid)) ? ssid : "Flock Falcon ALPR");
  const char* defVendor = (vendor && strlen(vendor)) ? vendor : "Flock Safety";
  const char* defVerdict = (verdict && strlen(verdict)) ? verdict : ((tier >= 3) ? "FLOCK_CONFIRMED" : "FLOCK_SUSPECT");
  uint8_t conf = (tier >= 4) ? 100 : ((tier >= 3) ? 85 : ((tier >= 2) ? 65 : 40));
  float distM = (rssi == 0) ? -1.0f : powf(10.0f, (-40.0f - (float)rssi) / 20.0f);
  int idx = wdfAddGenericDetection(mac, defName, "WiFi 2.4G", defVendor, method, defVerdict,
                                  category, tier, rssi, ch, distM, conf, outChirpWorthy);
  if (idx >= 0 && ssid && strlen(ssid)) {
    strlcpy(wdfDet[idx].ssid, ssid, sizeof(wdfDet[idx].ssid));
  }
  return idx;
}

// JSON-emit one detection line (rich multi-threat format) and broadcast via BLE.
static void emitDetectionJSON(const char* mac, const char* method, uint8_t tier,
                              uint8_t category, const char* name, const char* vendor,
                              const char* verdict, int8_t rssi, uint8_t ch) {
  char buf[320];
  snprintf(buf, sizeof(buf),
           "{\"event\":\"detection\",\"category\":\"%s\",\"name\":\"%s\","
           "\"vendor\":\"%s\",\"verdict\":\"%s\",\"detection_method\":\"wifi_%s\","
           "\"detection_tier\":%u,\"protocol\":\"wifi_2_4ghz\","
           "\"mac_address\":\"%s\",\"rssi\":%d,\"channel\":%u,"
           "\"frequency\":%u}",
           categoryToString((TargetCategory)category), name, vendor, verdict,
           method, (unsigned)tier, mac, rssi, (unsigned)ch,
           (unsigned)(ch ? (2407 + 5*ch) : 2402));
  Serial.println(buf);
  WhereDaFlockBLETelemetry::broadcast(buf);
}

// ---------------------------------------------------------------------------
// Per-iteration work in loop() context
// ---------------------------------------------------------------------------
static void drainAlertQueue() {
  size_t n = 0;
  while (alertHead != alertTail && n < ALERT_QUEUE_SIZE) {
    AlertEntry local;
    memcpy(&local, (const void*)&alertQueue[alertTail], sizeof(AlertEntry));
    const AlertEntry* e = &local;
    alertTail = (alertTail + 1) % ALERT_QUEUE_SIZE;
    n++;

    char mac[18];
    macToStr((const uint8_t*)e->mac, mac, sizeof(mac));
    const char* method = tierToMethodLetter(e->tier);

    bool chirpWorthy = false;
    const char* finalName = (e->name[0] != '\0') ? e->name : ((e->ssid[0] != '\0') ? e->ssid : "Target");
    const char* finalVendor = (e->vendor[0] != '\0') ? e->vendor : "Surveillance";
    const char* finalVerdict = (e->verdict[0] != '\0') ? e->verdict : "CONFIRMED";

    int idx = fyAddDetection(mac, finalName, finalVendor, method, finalVerdict,
                             e->category, e->tier, e->rssi, e->channel,
                             (const char*)e->ssid, &chirpWorthy);
    if (idx < 0) continue;
    if (chirpWorthy) {
      fyLastTargetSeen = millis();
      fyLastTargetTier = e->tier;
    }

    if (!shouldSuppressDuplicate(mac, e->tier)) {
      emitDetectionJSON(mac, method, e->tier, e->category, finalName, finalVendor, finalVerdict, e->rssi, e->channel);
      if (e->category == CAT_POLICE_VEHICLE || e->category == CAT_POLICE_BODYCAM || e->category == CAT_POLICE_RADIO) {
        policeChirp();
      } else {
        tierChirp(e->tier);
      }
      ledSet(true); ledOffAt = millis() + LED_FLASH_MS;
#ifdef USE_M5STICKC_PLUS_DISPLAY
      uint8_t conf = (e->tier >= 4) ? 100 : ((e->tier >= 3) ? 85 : ((e->tier >= 2) ? 65 : 40));
      float distM = (e->rssi == 0) ? -1.0f : powf(10.0f, (-40.0f - (float)e->rssi) / 20.0f);
      m5stickDisplayShowAlertRich("WiFi 2.4G", finalName, mac, finalVendor,
                                 method, finalVerdict, e->rssi, distM, conf,
                                 e->channel, ALERT_COOLDOWN_MS, e->category);
#else
      dongleDisplayShowAlert(method, mac, e->rssi, e->channel, ALERT_COOLDOWN_MS);
#endif
    }
  }
}

static void updateChannelMode() {
#if CHANNEL_MODE == CHANNEL_MODE_SINGLE
  if (currentChannel != SINGLE_CHANNEL) {
    currentChannel = SINGLE_CHANNEL;
    esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
  }
  #ifdef USE_M5STICKC_PLUS_DISPLAY
  m5stickDisplayShowIdle(currentChannel, wdfDetCount);
#else
  dongleDisplayShowIdle(currentChannel, wdfDetCount);
#endif
  return;
#else
  if (millis() - lastHop < CHANNEL_DWELL_MS) return;
  chanIdx = (chanIdx + 1) % activeChannelCount;
  currentChannel = activeChannels[chanIdx];
  esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
  lastHop = millis();
  #ifdef USE_M5STICKC_PLUS_DISPLAY
  m5stickDisplayShowIdle(currentChannel, wdfDetCount);
#else
  dongleDisplayShowIdle(currentChannel, wdfDetCount);
#endif
#endif
}

static void heartbeatTick() {
  if (millis() - fyLastTargetSeen > HB_DEVICE_ACTIVE_MS) return;
  if (millis() - fyLastHeartbeatAt >= HB_BEEP_INTERVAL_MS) {
    fyLastHeartbeatAt = millis();
    if (tierAudible(fyLastTargetTier)) heartbeatBeep();
  }
}

// ---------------------------------------------------------------------------
// Host command channel (USB CDC input). The dashboard sends one JSON command
// per line when it wants to mute tiers or pull an offline session.
//   {"cmd":"get_config"}
//   {"cmd":"set_beep","tier":N,"on":0|1}
//   {"cmd":"set_beep_mask","mask":0-31}
//   {"cmd":"dump_session","source":"live"|"prev"}
//   {"cmd":"clear_session"}
// ---------------------------------------------------------------------------
static char cmdBuf[128];
static size_t cmdLen = 0;

static void handleHostCommands() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (cmdLen > 0) {
        cmdBuf[cmdLen] = '\0';
        String cmd = String(cmdBuf);

        if (cmd.startsWith("RADIO ") || cmd.indexOf("radio_alert") >= 0) {
          // External Police Radio / Frequency Bridge parser
          // Examples:
          //   RADIO 851.250 P25_PHASE2 -62 PD DISPATCH TAC1
          //   {"cmd":"radio_alert","freq":851.25,"proto":"P25","rssi":-62,"desc":"PD TAC1"}
          char rProto[16] = "P25";
          char rDesc[32] = "Police Radio";
          float rFreq = 851.0f;
          int rRssi = -70;

          if (cmd.startsWith("RADIO ")) {
            char fStr[16] = {0};
            char pStr[16] = {0};
            char rStr[16] = {0};
            char dStr[32] = {0};
            int parsed = sscanf(cmdBuf, "RADIO %15s %15s %15s %31[^\r\n]", fStr, pStr, rStr, dStr);
            if (parsed >= 1) rFreq = atof(fStr);
            if (parsed >= 2) strncpy(rProto, pStr, sizeof(rProto) - 1);
            if (parsed >= 3) rRssi = atoi(rStr);
            if (parsed >= 4) strncpy(rDesc, dStr, sizeof(rDesc) - 1);
          } else {
            int fIdx = cmd.indexOf("\"freq\":");
            if (fIdx >= 0) rFreq = cmd.substring(fIdx + 7).toFloat();
            int rIdx = cmd.indexOf("\"rssi\":");
            if (rIdx >= 0) rRssi = cmd.substring(rIdx + 7).toInt();
            int pIdx = cmd.indexOf("\"proto\":\"");
            if (pIdx >= 0) {
              int pEnd = cmd.indexOf("\"", pIdx + 9);
              if (pEnd > pIdx + 9) {
                String sub = cmd.substring(pIdx + 9, pEnd);
                strncpy(rProto, sub.c_str(), sizeof(rProto) - 1);
              }
            }
            int dIdx = cmd.indexOf("\"desc\":\"");
            if (dIdx >= 0) {
              int dEnd = cmd.indexOf("\"", dIdx + 8);
              if (dEnd > dIdx + 8) {
                String sub = cmd.substring(dIdx + 8, dEnd);
                strncpy(rDesc, sub.c_str(), sizeof(rDesc) - 1);
              }
            }
          }

          char rMac[20];
          snprintf(rMac, sizeof(rMac), "RF:%.3f", rFreq);
          bool chirpWorthy = false;
          wdfAddGenericDetection(rMac, rDesc, "RADIO", rProto, "RF_BRIDGE", "POLICE_RADIO",
                                 CAT_POLICE_RADIO, 4, (int8_t)rRssi, 0, -1.0f, 95, &chirpWorthy);
          policeChirp();
          ledSet(true); ledOffAt = millis() + LED_FLASH_MS;
          char rJson[256];
          snprintf(rJson, sizeof(rJson),
                   "{\"event\":\"radio_detection\",\"freq_mhz\":%.3f,\"proto\":\"%s\","
                   "\"rssi\":%d,\"desc\":\"%s\",\"category\":\"POLICE RADIO\"}",
                   rFreq, rProto, rRssi, rDesc);
          Serial.println(rJson);
          WhereDaFlockBLETelemetry::broadcast(rJson);
#ifdef USE_M5STICKC_PLUS_DISPLAY
          m5stickDisplayShowAlertRich("RF BRIDGE", rDesc, rMac, rProto,
                                     "POLICE_FREQ", "RADIO_TX", (int8_t)rRssi, -1.0f, 95,
                                     0, 6000, CAT_POLICE_RADIO);
#endif
        } else if (cmd.indexOf("get_config") >= 0) {
          WhereDaFlockSession::emitConfigJSON();
        } else if (cmd.indexOf("set_beep_mask") >= 0) {
          int mask = cmd.substring(cmd.indexOf("mask\":") + 6).toInt();
          if (mask >= 0 && mask < 32) {
            wdfBeepMask = (uint8_t)mask;
            WhereDaFlockSession::saveBeepMask();
            WhereDaFlockSession::emitConfigJSON();
          }
        } else if (cmd.indexOf("set_beep") >= 0) {
          int tier = cmd.substring(cmd.indexOf("tier\":") + 6).toInt();
          bool on = cmd.indexOf("\"on\":1") >= 0;
          if (tier >= 0 && tier < TIER_COUNT) {
            if (on) wdfBeepMask |= (1 << tier); else wdfBeepMask &= ~(1 << tier);
            WhereDaFlockSession::saveBeepMask();
            WhereDaFlockSession::emitConfigJSON();
          }
        } else if (cmd.indexOf("dump_session") >= 0) {
          bool prev = cmd.indexOf("\"source\":\"prev\"") >= 0;
          WhereDaFlockSession::dumpSession(prev ? "prev" : "live");
        } else if (cmd.indexOf("clear_session") >= 0) {
          wdfDetCount = 0;
          Serial.println("{\"event\":\"cleared\"}");
        }
        cmdLen = 0;
      }
    } else if (cmdLen < sizeof(cmdBuf) - 1) {
      cmdBuf[cmdLen++] = c;
    }
  }
}

// ---------------------------------------------------------------------------
// Arduino
// ---------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(200);

  // Board-neutral LED/buzzer init (raw GPIO on generic ESP32, M5Unified on
  // M5Stack boards).
  wdf_hal::ledInit();
  wdf_hal::ledSet(false);
  wdf_hal::buzzerInit();

  // Pre-compile OUIs into byte table (kept in IRAM).
  for (size_t i = 0; i < ALL_OUI_COUNT; i++) {
    all_oui_bytes[i][0] = (uint8_t)strtol(ALL_TARGET_OUIS[i].oui,     NULL, 16);
    all_oui_bytes[i][1] = (uint8_t)strtol(ALL_TARGET_OUIS[i].oui + 3, NULL, 16);
    all_oui_bytes[i][2] = (uint8_t)strtol(ALL_TARGET_OUIS[i].oui + 6, NULL, 16);
  }

  Serial.println("WhereDaFlock v2.2.0 - passive Multi-Threat Surveillance Detector");
  Serial.println("RECEIVE-ONLY promiscuous mode. No transmissions.");
  Serial.printf("Targeting %u Multi-Threat Surveillance Signatures (%s)\n", (unsigned)ALL_OUI_COUNT, __DATE__);
  WhereDaFlockBLE::bleSetup();   // no-op unless WDF_ENABLE_BLE; prints a note if on

  startupBeep();
  #ifdef USE_M5STICKC_PLUS_DISPLAY
  m5stickDisplayInit();
  m5stickDisplayShowBoot();
#else
  dongleDisplayInit();
#endif
  WhereDaFlockBLETelemetry::init();

  // Session + control plane (SPIFFS persistence, NVS beep mask, boot recovery).
  WhereDaFlockSession::loadBeepMask();
  if (SPIFFS.begin(true)) WhereDaFlockSession::promotePrevSession();
  WhereDaFlockSession::emitConfigJSON();

  // Enable promiscuous mode over the STA radio (receive-only; we never
  // associate to a network, so no connection is ever made).
  WiFi.mode(WIFI_MODE_STA);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&wifiSniffer);
  wifi_promiscuous_filter_t f = {};
  f.filter_mask = WIFI_PROMIS_FILTER_MASK_ALL;
  esp_wifi_set_promiscuous_filter(&f);

#if CHANNEL_MODE == CHANNEL_MODE_SINGLE
  (void)activeChannels; (void)activeChannelCount;
  currentChannel = SINGLE_CHANNEL;
#elif CHANNEL_MODE == CHANNEL_MODE_FULL_HOP
  activeChannels = fullHopChannels;
  activeChannelCount = fullHopChannelCount;
  currentChannel = activeChannels[0];
#else
  activeChannels = customChannels;
  activeChannelCount = customChannelCount;
  currentChannel = activeChannels[0];
#endif
  esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
  lastHop = millis();

  Serial.println("Scanning ...");
}

void loop() {
  WhereDaFlockBLE::checkAlerts();
  if (WhereDaFlockBLE::blePoll()) {   // BLE scan window owns the radio
    WhereDaFlockBLE::checkAlerts();
    #ifdef USE_M5STICKC_PLUS_DISPLAY
    m5stickDisplayTick(millis(), currentChannel, wdfDetCount);
    #else
    dongleDisplayTick(millis(), currentChannel, wdfDetCount);
    #endif
    handleHostCommands();
    ledTick();
    delay(1);
    return;
  }
  drainAlertQueue();
  updateChannelMode();
  heartbeatTick();
  WhereDaFlockBLE::checkAlerts();
  #ifdef USE_M5STICKC_PLUS_DISPLAY
  m5stickDisplayTick(millis(), currentChannel, wdfDetCount);
#else
  dongleDisplayTick(millis(), currentChannel, wdfDetCount);
#endif
  handleHostCommands();          // dashboard control plane (USB CDC)
  ledTick();
  delay(1);

  if (millis() - fyLastHeartbeatAt >= HEARTBEAT_MS) {
    fyLastHeartbeatAt = millis();
    Serial.printf("[wdf] scanning ch=%u det=%d\n", currentChannel, wdfDetCount);
  }

  // Autosave session to SPIFFS every 60s when the table changed.
  static unsigned long lastSaveAt = 0;
  if (millis() - lastSaveAt >= 60000) {
    lastSaveAt = millis();
    WhereDaFlockSession::saveSession();
  }
}