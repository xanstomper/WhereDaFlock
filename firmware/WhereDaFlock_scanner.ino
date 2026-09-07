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
#include "src/display_dongle.h"

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
static uint8_t oui_bytes[OUI_COUNT][3];

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
  uint8_t mac[6];
  int8_t  rssi;
  uint8_t channel;
  bool    wildcardProbe;
  char    ssid[33];
  char    frameKind[12];
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
static inline void ledSet(bool on) {
#if LED_ACTIVE_HIGH
  digitalWrite(LED_PIN, on ? HIGH : LOW);
#else
  digitalWrite(LED_PIN, on ? LOW : HIGH);
#endif
}
static void ledTick() {
  if (ledOffAt && (long)(millis() - ledOffAt) >= 0) { ledSet(false); ledOffAt = 0; }
}
static inline bool tierAudible(uint8_t tier) {
  return tier < TIER_COUNT && ((wdfBeepMask >> tier) & 0x01);
}
static void blip(uint16_t hz) { tone(BUZZER_PIN, hz); delay(BLIP_MS); noTone(BUZZER_PIN); }
static void chirp2(uint16_t lo, uint16_t hi) {
  tone(BUZZER_PIN, lo); delay(TIER_NOTE_MS); noTone(BUZZER_PIN);
  delay(TIER_GAP_MS);
  tone(BUZZER_PIN, hi); delay(TIER_NOTE_MS); noTone(BUZZER_PIN);
}
static void tierChirp(uint8_t tier) {
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
static void heartbeatBeep() {
  tone(BUZZER_PIN, HB_BEEP_HZ); delay(HB_NOTE_MS); noTone(BUZZER_PIN);
  delay(HB_GAP_MS);
  tone(BUZZER_PIN, HB_BEEP_HZ); delay(HB_NOTE_MS); noTone(BUZZER_PIN);
}

static void startupBeep() {
#if USE_BUZZER
  // First 6 notes of SMB World 1-2 (underground). Koji Kondo's descending
  // pattern: C4, C5, A3, A4, B♭3, B♭4 (alternating-octave pairs).
  static const uint16_t notes[6] = { 262, 523, 220, 440, 233, 466 };
  for (int i = 0; i < 6; i++) {
    tone(BUZZER_PIN, notes[i]);
    delay((i == 5) ? 160 : 95);
    noTone(BUZZER_PIN);
    if (i < 5) delay(22);
  }
#endif
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static inline bool isMulticast(const uint8_t* mac) { return mac[0] & 0x01; }

static bool IRAM_ATTR matchOuiRaw(const uint8_t* mac) {
  // Skip broadcast/multicast. We do NOT skip locally-administered: 82:6b:f2
  // has bit 1 of byte 0 set, so that would drop a real camera.
  for (size_t i = 0; i < OUI_COUNT; i++) {
    if (mac[0] == oui_bytes[i][0] && mac[1] == oui_bytes[i][1] && mac[2] == oui_bytes[i][2])
      return true;
  }
  return false;
}

static void macToStr(const uint8_t* mac, char* buf, size_t len) {
  snprintf(buf, len, "%02x:%02x:%02x:%02x:%02x:%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// Check the 802.11 header + trailer for a management probe request with a
// wildcard SSID (tag 0, length 0) and, on the top tier, the Flock IE
// fingerprint. This is a minimal, robust scanner of the probe body.
// fpPayload, fpLen hold the body bytes after the fixed header.
static bool IRAM_ATTR isWildcardProbe(const wifi_ieee80211_mac_hdr_t* hdr,
                                       const uint8_t* body, size_t bodyLen,
                                       bool* outHasFlockIe) {
  *outHasFlockIe = false;
  uint16_t fc = hdr->frame_ctrl;
  // Management frame (type 0), subtype 4 = Probe Request.
  if ((fc & 0x000C) != 0x0000) return false;
  if (((fc >> 4) & 0x0F) != 0x04) return false;

  // Walk the fixed fields of a probe request body: SSID element first.
  // Probe Request body (after fixed hdr): SSID (tag 0), Supported Rates (1),
  // then optional IEs.
  const uint8_t* p = body;
  const uint8_t* end = body + bodyLen;
  if (end - p < 2) return false;
  uint8_t tag = p[0], len = p[1];
  bool wildcard = (tag == 0 && len == 0);
  // Advance past SSID element (+ any zero-fill), bounded by the frame end.
  p += 2 + len;
  if (p > end) p = end;
  // Scan remaining IEs for the Flock fingerprint markers.
  while (p + 2 <= end) {
    uint8_t t = p[0], l = p[1];
    if (p + 2 + l > end) break;
    if (t == 0xFF && l > 0) *outHasFlockIe = true;   // vendor-specific element
    p += 2 + l;
  }
  return wildcard;
}

// ---------------------------------------------------------------------------
// Ring buffer helpers
// ---------------------------------------------------------------------------
static void IRAM_ATTR enqueueAlert(uint8_t tier, const uint8_t* mac, int8_t rssi,
                                    uint8_t ch, bool wc, const char* ssid, const char* kind) {
  portENTER_CRITICAL_ISR(&queueMux);
  size_t next = (alertHead + 1) % ALERT_QUEUE_SIZE;
  if (next == alertTail) { portEXIT_CRITICAL_ISR(&queueMux); return; } // full: drop
  AlertEntry* e = (AlertEntry*)&alertQueue[alertHead];
  e->tier = tier; e->rssi = rssi; e->channel = ch; e->wildcardProbe = wc;
  memcpy((void*)e->mac, mac, 6);
  if (ssid) strncpy((char*)e->ssid, ssid, 32);
  e->ssid[32] = '\0';
  if (kind) strncpy((char*)e->frameKind, kind, 11);
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

  // addr2 (transmitter) OUI match - primary tier-2 signal.
  if (!isMulticast(hdr->addr2) && matchOuiRaw(hdr->addr2)) {
    uint8_t tier = TIER_OUI;
    bool wc = false, hasIe = false;
    if (type == WIFI_PKT_MGMT) {
      const uint8_t* body = pkt->payload + sizeof(wifi_ieee80211_mac_hdr_t);
      size_t bodyLen = pkt->rx_ctrl.sig_len > sizeof(wifi_ieee80211_mac_hdr_t)
                       ? pkt->rx_ctrl.sig_len - sizeof(wifi_ieee80211_mac_hdr_t) : 0;
      if (isWildcardProbe(hdr, body, bodyLen, &hasIe)) {
        wc = true;
        tier = hasIe ? TIER_IE_SIG : TIER_PROBE;
      }
    }
    enqueueAlert(tier, hdr->addr2, (int8_t)pkt->rx_ctrl.rssi,
                 currentChannel, wc, "", "mgmt");
    return;
  }

  // addr1 (receiver) and addr3 (BSSID) OUI - tier-1 echo signals (noisier).
#if CHECK_ADDR1
  if (!isMulticast(hdr->addr1) && matchOuiRaw(hdr->addr1)) {
    enqueueAlert(TIER_ECHO, hdr->addr1, (int8_t)pkt->rx_ctrl.rssi,
                 currentChannel, false, "", "addr1");
  }
#endif
#if CHECK_ADDR3
  if (matchOuiRaw(hdr->addr3)) {
    enqueueAlert(TIER_ECHO, hdr->addr3, (int8_t)pkt->rx_ctrl.rssi,
                 currentChannel, false, "", "addr3");
  }
#endif

#if ENABLE_SSID_MATCH
  (void)target_ssid_keywords; // SSID keyword path (off by default)
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

static int fyAddDetection(const char* mac, const char* method, uint8_t tier,
                          int8_t rssi, uint8_t ch, bool* outChirpWorthy) {
  uint32_t now = millis();
  for (int i = 0; i < wdfDetCount; i++) {
    if (strcmp(wdfDet[i].mac, mac) == 0) {
      bool rediscover = (now - wdfDet[i].lastSeen) > REDISCOVER_MS;
      if (wdfDet[i].count < 0xFFFF) wdfDet[i].count++;
      wdfDet[i].lastSeen = now;
      wdfDet[i].rssi = rssi;
      wdfDet[i].channel = ch;
      bool upgrade = tier > wdfDet[i].tier;
      if (upgrade) {
        wdfDet[i].tier = tier;
        strlcpy(wdfDet[i].method, method ? method : "", sizeof(wdfDet[i].method));
      }
      if (outChirpWorthy) *outChirpWorthy = rediscover || upgrade;
      return i;
    }
  }
  if (wdfDetCount >= MAX_DETECTIONS) { if (outChirpWorthy) *outChirpWorthy = false; return -1; }
  WDFDetection& d = wdfDet[wdfDetCount];
  strlcpy(d.mac, mac, sizeof(d.mac));
  strlcpy(d.method, method ? method : "", sizeof(d.method));
  d.tier = tier; d.rssi = rssi; d.channel = ch;
  d.firstSeen = d.lastSeen = now; d.count = 1; d.ssid[0] = '\0';
  wdfDetCount++;
  if (outChirpWorthy) *outChirpWorthy = true;
  return wdfDetCount - 1;
}

// JSON-emit one detection line (manual, compact).
static void emitDetectionJSON(const char* mac, const char* method, uint8_t tier,
                              int8_t rssi, uint8_t ch) {
  Serial.printf("{\"event\":\"detection\",\"detection_method\":\"wifi_%s\","
                "\"detection_tier\":%u,\"protocol\":\"wifi_2_4ghz\","
                "\"mac_address\":\"%s\",\"rssi\":%d,\"channel\":%u,"
                "\"frequency\":%u,\"ssid\":\"\"}\n",
                method, (unsigned)tier, mac, rssi, (unsigned)ch, (unsigned)(2407 + 5*ch));
}

// ---------------------------------------------------------------------------
// Per-iteration work in loop() context
// ---------------------------------------------------------------------------
static void drainAlertQueue() {
  size_t n = 0;
  while (alertHead != alertTail && n < ALERT_QUEUE_SIZE) {
    const AlertEntry* e = &alertQueue[alertTail];
    alertTail = (alertTail + 1) % ALERT_QUEUE_SIZE;
    n++;

    char mac[18];
    macToStr(e->mac, mac, sizeof(mac));
    const char* method = tierToMethodLetter(e->tier);

    bool chirpWorthy = false;
    int idx = fyAddDetection(mac, method, e->tier, e->rssi, e->channel, &chirpWorthy);
    if (idx < 0) continue;
    if (chirpWorthy) {
      fyLastTargetSeen = millis();
      fyLastTargetTier = e->tier;
    }

    if (!shouldSuppressDuplicate(mac, e->tier)) {
      emitDetectionJSON(mac, method, e->tier, e->rssi, e->channel);
      tierChirp(e->tier);
      ledSet(true); ledOffAt = millis() + LED_FLASH_MS;
      dongleDisplayShowAlert(method, mac, e->rssi, e->channel, ALERT_COOLDOWN_MS);
    }
  }
}

static void updateChannelMode() {
#if CHANNEL_MODE == CHANNEL_MODE_SINGLE
  if (currentChannel != SINGLE_CHANNEL) {
    currentChannel = SINGLE_CHANNEL;
    esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
  }
  dongleDisplayShowIdle(currentChannel, wdfDetCount);
  return;
#else
  if (millis() - lastHop < CHANNEL_DWELL_MS) return;
  chanIdx = (chanIdx + 1) % activeChannelCount;
  currentChannel = activeChannels[chanIdx];
  esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
  lastHop = millis();
  dongleDisplayShowIdle(currentChannel, wdfDetCount);
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

        if (cmd.indexOf("get_config") >= 0) {
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

  pinMode(LED_PIN, OUTPUT);
  ledSet(false);
  pinMode(BUZZER_PIN, OUTPUT);

  // Pre-compile OUIs into byte table (kept in IRAM). Note: 82:6b:f2 is kept;
  // do not add a locally-administered skip - it would drop a real camera.
  for (size_t i = 0; i < OUI_COUNT; i++) {
    oui_bytes[i][0] = (uint8_t)strtol(TARGET_OUIS[i],    NULL, 16);
    oui_bytes[i][1] = (uint8_t)strtol(TARGET_OUIS[i] + 3, NULL, 16);
    oui_bytes[i][2] = (uint8_t)strtol(TARGET_OUIS[i] + 6, NULL, 16);
  }

  Serial.println("WhereDaFlock v2.1.0 - passive 2.4GHz Flock Cam detector");
  Serial.println("RECEIVE-ONLY promiscuous mode. No transmissions.");
  Serial.printf("Targeting %u Flock OUIs (%s)\n", (unsigned)OUI_COUNT, __DATE__);

  startupBeep();
  dongleDisplayInit();

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
  drainAlertQueue();
  updateChannelMode();
  heartbeatTick();
  dongleDisplayTick(millis(), currentChannel, wdfDetCount);
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