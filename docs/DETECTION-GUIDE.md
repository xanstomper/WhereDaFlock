# WhereDaFlock — Detection Guide

A deep technical reference for how the WhereDaFlock detector identifies Flock
Cam surveillance devices. This document is for researchers and builders who
want to understand the *why* behind the firmware, tune it, or extend it.

---

## 1. What a Flock Cam emits and why

Flock Safety ALPR / edge cameras are battery- or solar-powered devices that
upload plate detections over cellular LTE. To do their job they also need to
discover and maintain a network presence. Over time, their radio behavior has
changed:

| Period | Observable signal | Status |
|--------|-------------------|--------|
| ≤ Dec 2025 | A WiFi **management access point** broadcast for administration | Deactivated ~Dec 2025 |
| 2025 → spring 2026 | **BLE** maintenance/diagnostic beacons | Stopped working spring 2026 |
| spring 2026 → now | **802.11 wildcard probe requests** on ascending channels | **Current & working** |

The most useful current signal is the **wildcard probe request**. A probe
request is a WiFi management frame (type `0x00`, subtype `0x04`) that a station
sends to find nearby access points. A *wildcard* probe carries an SSID
Information Element with **length 0**, meaning "any AP, respond with your
SSID."

Why would a camera send these? In STA (station) mode, the camera continuously
enumerates nearby networks — likely as default behavior rather than something
Flock explicitly configured. The result is a reliable, passive, and
fingerprintable emission.

---

## 2. The 802.11 frame anatomy (probe request)

A captured management frame looks like this at the byte level:

```
 Offset  Size  Field
 ------  ----  --------------------------------------------------
   0       2   Frame Control        (bits 2-3 = type, bits 4-7 = subtype)
   2       2   Duration / ID
   4       6   addr1  destination  (broadcast: ff:ff:ff:ff:ff:ff)
  10       6   addr2  transmitter  (the CAMERA's MAC — this is the key)
  16       6   addr3  BSSID
  22       2   Sequence Control
  24       ...  Frame Body (management-specific)
        ...       SSID element    tag=0, len=N        <- N==0 means wildcard
        ...       Supported Rates tag=1, len=N
        ...       other IEs       vendor elements etc. <- fingerprint markers
```

WhereDaFlock's sniffer parses the fixed header with this packed C struct:

```c
typedef struct __attribute__((packed)) {
  uint16_t frame_ctrl;
  uint16_t duration;
  uint8_t  addr1[6];
  uint8_t  addr2[6];
  uint8_t  addr3[6];
  uint16_t seq_ctrl;
} wifi_ieee80211_mac_hdr_t;
```

### 2.1 Frame Control bits

- **Type** = `(frame_ctrl >> 2) & 0x3` → `0` = management
- **Subtype** = `(frame_ctrl >> 4) & 0xF` → `4` = probe request

The firmware checks both to confirm a probe request before inspecting the body.

---

## 3. Three matching signals

Every observed frame is scored against three signals. They are deliberately
weighted so a single weak signal (a common OUI, for example) is not enough to
trigger a false alert.

### 3.1 Transmitter OUI (`addr2`)

The first three bytes of the transmitter MAC are the Organizationally Unique
Identifier (OUI) assigned to the chipset vendor. Flock cameras use radios whose
OUI appears in a curated 32-entry list:

```
70:c9:4e  3c:91:80  d8:f3:bc  80:30:49  b8:35:32
14:5a:fc  74:4c:a1  08:3a:88  9c:2f:9d  c0:35:32
94:08:53  e4:aa:ea  f4:6a:dd  e0:0a:f6  24:b2:b9
00:f4:8d  d0:39:57  e8:d0:fc  e0:4f:43  b8:1e:a4
70:08:94  58:8e:81  ec:1b:bd  3c:71:bf  58:00:e3
90:35:ea  5c:93:a2  64:6e:69  48:27:ea  a4:cf:12
14:b5:cd
82:6b:f2   ← contributed by DeFlockJoplin
```

> **Locally-administered MACs.** Normally MACs with `bit 1` of byte 0 set are
> locally-administered (randomized) and worth skipping. The matcher here
> deliberately does **not** skip them, because `82:6b:f2` has that bit set.
> Adding such a filter would silently drop a real Flock camera.

To keep the match fast (the callback runs in the WiFi task and must never block
or allocate), the OUI list is pre-compiled once into a byte table in `setup()`:

```c
static uint8_t oui_bytes[OUI_COUNT][3];
// oui_bytes[i][0..2] = parsed bytes of target_ouis[i]
```

### 3.2 Wildcard SSID element

Inside the probe-request body, the **first** Information Element is the SSID:

```c
uint8_t tag = body[0];   // 0 == SSID
uint8_t len = body[1];   // 0 == wildcard
bool wildcard = (tag == 0 && len == 0);
```

A wildcard probe is necessary for the strong tiers (3 and 4).

### 3.3 Information-Element fingerprint

Beyond the wildcard SSID, the frame body may carry **vendor-specific
Information Elements** (`tag == 0xFF`). The presence and shape of these IEs form
the fingerprint. The top confidence tier (`wildcard_probe_ie_sig`) requires a
vendor IE to be present. Per DeFlockJoplin's drive-testing, this combination
(the wildcard probe + known OUI + IE fingerprint) showed **no false positives**
across hundreds of miles.

---

## 4. Confidence tiers

All signal paths run simultaneously. Every detection is labeled by its
**highest-confidence tier**, which decides the audio and the method string that
wins for that MAC. A broad hit can never overwrite a fingerprint-confirmed one.

| Tier | `detection_method` | Gate | Sound |
|------|--------------------|------|-------|
| **4** | `wifi_wildcard_probe_ie_sig` | OUI + wildcard SSID + IE fingerprint | two-note ascending 2000→2800 Hz |
| **3** | `wifi_wildcard_probe` | OUI + wildcard SSID, IE unverified | two-note ascending 1400→1800 Hz |
| **2** | `wifi_oui_addr2` | transmitter-side OUI on any frame | single blip 1200 Hz |
| **1** | `wifi_oui_addr1` / `wifi_oui_addr3` | receiver / BSSID OUI (AP echo) | single blip 800 Hz |
| **0** | `wifi_ssid` | SSID keyword (off by default) | single blip 600 Hz |

### Why tier 1 exists

When a Flock camera sends a wildcard probe, nearby access points **answer** it.
That answer carries the *camera's* MAC in `addr1` (receiver field). So a
`addr1` OUI match is an *echo* of the camera, not the camera itself — useful for
covering stations that are silent during the device's dwell window, but noisier.

---

## 5. Runtime architecture

The ESP32's radio runs a promiscuous callback in the WiFi task. That task has
hard real-time constraints: it must not call `Serial.print`, `malloc`, or do
slow I/O. So the firmware splits work:

```
 [2.4GHz air]
      │
      ▼
 wifiSniffer() ────────── IRAM promiscuous callback (WiFi task)
      │                    fast OUI match only; enqueues only
      ▼
 alertQueue[32] ───────── lock-free ring buffer (portMUX-protected)
      │
      ▼
 drainAlertQueue() ────── loop() context; per-iteration drain
      │
      ├──► fyAddDetection()   keep best tier per MAC (dedupe table)
      ├──► tierChirp(tier)    distinct audio per tier
      ├──► emitDetectionJSON() one JSON line on USB/serial
      └──► shouldSuppressDuplicate() 5s per-MAC rate limit; higher tier
                                     preempts the cooldown
```

### 5.1 IRAM callback safety

- No `Serial` calls in the callback.
- No dynamic allocation (`malloc`) in the callback.
- The ring buffer is a fixed-size array guarded by `portENTER_CRITICAL_ISR`.
- If the buffer is full, the frame is dropped (loop() is simply behind).

### 5.2 Channel strategy

Flock cameras hop channels in **ascending** order (~0.125 s dwell). The sniffer
hops the **reverse** order (`11 → 6 → 1`) with a 250 ms dwell, giving roughly
twice the camera's hop time on each channel to intercept a probe. This is a
deliberate signal-detection trade: faster than the camera's change so a probe
isn't missed.

---

## 6. False-positive analysis

| Scenario | Tier reached | Why it's handled |
|----------|--------------|------------------|
| Unrelated IoT device sharing a common OUI, no wildcard probe | 2 | OUI alone is only tier 2 (low blip); needs a probe for more |
| Any AP answering a probe (dest = camera) | 1 | `addr1` path is explicitly tier 1 (widest net) |
| Device with a wildcard probe + known OUI but **no** IE fingerprint | 3 | Not confirmed; could be on older firmware or sharing the OUI |
| Device with wildcard probe + known OUI + vendor IE | 4 | **Confirm best** — drive-testing found 0 false positives |

The design accepts that tier 1–3 are *candidates* and reserves tier 4 for the
high-confidence lock.

---

## 7. Extending the signature set

To add a new OUI:

1. Add it (lowercase, colons) to `TARGET_OUIS[]` in
   `firmware/src/signatures.h`.
2. Add the same OUI to `TARGET_OUIS` in `firmware/host_scanner.py`.
3. Add a matching assertion in `firmware/tests/test_detection.py`.
4. Re-run: `python3 firmware/tests/test_detection.py`.

The byte table is rebuilt automatically on boot; no OTA or re-partition needed.

---

## 8. References

- **@NitekryDPaul / OrdoOuroboros** — Flock Cam OUI research and the `addr1`
  detection technique ([nite-oui-collection](https://github.com/nitekry/nite-oui-collection)).
- **DeFlockJoplin** — wildcard-probe + IE fingerprint and the `82:6b:f2` OUI.
- **colonelpanichacks** — [flock-you](https://github.com/colonelpanichacks/flock-you),
  the firmware whose pipeline this project models.
- **Lucia Pintor & Luigi Atzori** — *Analysis of Wi-Fi Probe Requests Towards
  Information Element Fingerprinting* (IEEE GLOBECOM 2022).
- **nsm_barii** — observation of Flock camera probe-hop timing.