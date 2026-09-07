// WhereDaFlock - session persistence, NVS beep mask, and host command channel.
//
// Pulls the on-device + host-control features of colonelpanichacks/flock-you
// into the WhereDaFlock WiFi detector:
//   * CRC-validated SPIFFS session persistence (/session.json + /prev_session.json)
//   * NVS-persisted per-tier audio mute mask
//   * USB-CDC host command channel so the dashboard can query the device,
//     mute individual tiers, and pull standalone (offline) sessions.
//
// RECEIVE-ONLY radio remains unaffected; this module only affects on-device
// storage and the USB serial control plane.

#ifndef WHERE_DA_FLOCK_SESSION_H
#define WHERE_DA_FLOCK_SESSION_H

#include <Arduino.h>
#include <SPIFFS.h>
#include <Preferences.h>
#include "signatures.h"

using namespace WhereDaFlock;

// ---------------------------------------------------------------------------
// Detection table - defined in the firmware sketch with EXTERNAL linkage, so
// the session module and the sketch share the same table. Bit N of
// wdfBeepMask is tier N audible. Declared here at global scope.
// ---------------------------------------------------------------------------
#ifndef WDF_MAX_DETECTIONS
#define WDF_MAX_DETECTIONS 200
#endif

typedef struct {
  char     mac[18];
  char     name[32];
  char     protocol[10];
  char     vendor[32];
  char     method[24];
  char     verdict[24];
  uint8_t  category;
  uint8_t  tier;
  int8_t   rssi;
  float    distM;
  uint8_t  confidence;
  uint8_t  channel;
  uint32_t firstSeen;
  uint32_t lastSeen;
  uint16_t count;
  char     ssid[33];
} WDFDetection;

extern WDFDetection wdfDet[WDF_MAX_DETECTIONS];
extern int          wdfDetCount;
extern volatile uint8_t wdfBeepMask;   // bit N = tier N audible

void tierChirp(uint8_t tier);
void policeChirp();

int wdfAddBleDetection(const char* mac, const char* name, const char* vendor,
                       const char* method, const char* verdict, uint8_t category,
                       int8_t rssi, float distM, uint8_t conf, bool* outChirpWorthy);

int wdfAddGenericDetection(const char* mac, const char* name, const char* proto,
                           const char* vendor, const char* method, const char* verdict,
                           uint8_t category, uint8_t tier, int8_t rssi, uint8_t ch,
                           float distM, uint8_t conf, bool* outChirpWorthy);

namespace WhereDaFlockSession {

#define FY_SESSION_FILE  "/session.json"
#define FY_SESSION_TMP   "/session.tmp"
#define FY_PREV_FILE     "/prev_session.json"
#define FY_NVS_NS        "wdf"
#define FY_NVS_BEEP      "beepmask"

// ---------------------------------------------------------------------------
// CRC-32 (zlib / standard 0xEDB88320 polynomial)
// ---------------------------------------------------------------------------
inline uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t len) {
  crc = ~crc;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int k = 0; k < 8; k++)
      crc = (crc >> 1) ^ (0xEDB88320u & -(int32_t)(crc & 1));
  }
  return ~crc;
}

// ---------------------------------------------------------------------------
// Beep mask (NVS)
// ---------------------------------------------------------------------------
inline void loadBeepMask(uint8_t def = 0x1F) {
  Preferences p;
  if (!p.begin(FY_NVS_NS, true)) { wdfBeepMask = def; return; }
  wdfBeepMask = p.getUChar(FY_NVS_BEEP, def);
  p.end();
}
inline void saveBeepMask() {
  Preferences p;
  if (!p.begin(FY_NVS_NS, false)) return;
  p.putUChar(FY_NVS_BEEP, (uint8_t)wdfBeepMask);
  p.end();
}

// ---------------------------------------------------------------------------
// Serialize one detection to a JSON object string (no trailing comma)
// ---------------------------------------------------------------------------
inline size_t serializeDet(const WDFDetection& d, char* dst, size_t cap) {
  int n = snprintf(
      dst, cap,
      "{\"mac\":\"%s\",\"name\":\"%s\",\"protocol\":\"%s\",\"vendor\":\"%s\","
      "\"method\":\"%s\",\"verdict\":\"%s\",\"category\":%u,\"tier\":%u,\"rssi\":%d,"
      "\"channel\":%u,\"dist_m\":%.2f,\"conf\":%u,\"first\":%lu,\"last\":%lu,\"count\":%u,\"ssid\":\"%s\"}",
      d.mac, d.name, d.protocol, d.vendor, d.method, d.verdict, (unsigned)d.category,
      (unsigned)d.tier, d.rssi, (unsigned)d.channel, d.distM, (unsigned)d.confidence,
      (unsigned long)d.firstSeen, (unsigned long)d.lastSeen, (unsigned)d.count, d.ssid);
  return (n > 0 && (size_t)n < cap) ? (size_t)n : 0;
}

// ---------------------------------------------------------------------------
// SPIFFS session persistence
// ---------------------------------------------------------------------------
inline void promotePrevSession() {
  const char* src = nullptr;
  // Validate main, then tmp; copy whichever is valid to prev_session.json.
  for (int pass = 0; pass < 2; pass++) {
    const char* path = pass == 0 ? FY_SESSION_FILE : FY_SESSION_TMP;
    if (!SPIFFS.exists(path)) continue;
    File f = SPIFFS.open(path, "r");
    if (!f) continue;
    if (f.size() < 8) { f.close(); continue; }
    f.close();
    src = path;
    break;
  }
  if (!src) { SPIFFS.remove(FY_SESSION_FILE); SPIFFS.remove(FY_SESSION_TMP); return; }
  File s = SPIFFS.open(src, "r");
  File d = SPIFFS.open(FY_PREV_FILE, "w");
  if (s && d) {
    uint8_t buf[256]; int n;
    while ((n = s.read(buf, sizeof(buf))) > 0) d.write(buf, (size_t)n);
  }
  if (s) s.close(); if (d) d.close();
  SPIFFS.remove(FY_SESSION_FILE);
  SPIFFS.remove(FY_SESSION_TMP);
}

inline bool validateSessionFile(const char* path) {
  if (!SPIFFS.exists(path)) return false;
  File f = SPIFFS.open(path, "r");
  if (!f) return false;
  size_t sz = f.size();
  if (sz < 8) { f.close(); return false; }
  // Expected shape: line 1 is an envelope json "{...}\n", then a payload array.
  uint8_t first = 0;
  if (f.read(&first, 1) != 1 || first != '{') { f.close(); return false; }
  f.close();
  return true;
}

inline bool saveSession() {
  if (!SPIFFS.begin(true)) return false;

  // Envelope + payload to /session.tmp
  File f = SPIFFS.open(FY_SESSION_TMP, "w");
  if (!f) return false;
  f.printf("{\"v\":1,\"count\":%d}\n", wdfDetCount);
  char line[384];
  f.write((uint8_t*)"[", 1);
  for (int i = 0; i < wdfDetCount; i++) {
    if (i > 0) f.write((uint8_t*)",", 1);
    size_t n = serializeDet(wdfDet[i], line, sizeof(line));
    if (n) f.write((uint8_t*)line, n);
  }
  f.write((uint8_t*)"]", 1);
  f.close();

  // Atomic promote: remove main, rename tmp -> main
  SPIFFS.remove(FY_SESSION_FILE);
  SPIFFS.rename(FY_SESSION_TMP, FY_SESSION_FILE);
  return true;
}

inline void dumpSession(const char* source) {
  Serial.printf("{\"event\":\"session_begin\",\"source\":\"%s\",\"count\":%d}\n",
                source, wdfDetCount);
  char line[384];
  for (int i = 0; i < wdfDetCount; i++) {
    if (serializeDet(wdfDet[i], line, sizeof(line))) {
      Serial.printf("{\"event\":\"session_det\",%s}\n", line + 1); // splice event after '{'
    }
  }
  Serial.printf("{\"event\":\"session_end\",\"source\":\"%s\",\"count\":%d}\n",
                source, wdfDetCount);
}

inline void emitConfigJSON() {
  Serial.printf("{\"event\":\"config\",\"beep_mask\":%u,\"oui_count\":%u,"
                "\"tiers\":[{\"tier\":0,\"method\":\"wifi_ssid\",\"beep\":%u},"
                "{\"tier\":1,\"method\":\"wifi_oui_addr1_addr3\",\"beep\":%u},"
                "{\"tier\":2,\"method\":\"wifi_oui_addr2\",\"beep\":%u},"
                "{\"tier\":3,\"method\":\"wifi_wildcard_probe\",\"beep\":%u},"
                "{\"tier\":4,\"method\":\"wifi_wildcard_probe_ie_sig\",\"beep\":%u}]}\n",
                (unsigned)wdfBeepMask, (unsigned)OUI_COUNT,
                (wdfBeepMask >> 0) & 1, (wdfBeepMask >> 1) & 1,
                (wdfBeepMask >> 2) & 1, (wdfBeepMask >> 3) & 1,
                (wdfBeepMask >> 4) & 1);
}

} // namespace WhereDaFlockSession

#endif // WHERE_DA_FLOCK_SESSION_H