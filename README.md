# WhereDaFlock 🛡️

**Navigation freedom, without being watched.**

WhereDaFlock is two things working together:

1. **A privacy-forward iOS app** that maps known surveillance infrastructure,
   scores safer alternative routes, and alerts you as you travel.
2. **A receive-only ESP32 hardware detector** that passively detects Flock Cam
   and companion surveillance devices by their 2.4GHz radio emissions.

> **Tagline:** Navigate freely. Stay unseen.

---

## What's in this repo

| Path | What it is |
|------|-----------|
| `WhereDaFlock/` | iOS SwiftUI app (map, navigation, community reports, scanner) |
| `firmware/` | ESP32 passive 2.4GHz Flock Cam detector + host companion |
| `Backend/` | Optional Go backend (placeholder) |
| `WhereDaFlockTests/`, `WhereDaFlockUITests/`, `WhereDaFlockWatch/`, `WhereDaFlockWidgets/` | iOS companion scaffolding |

---

## 📱 The app

A privacy-focused navigation assistant showing known surveillance
infrastructure, community alerts, and safer alternative routes.

- **Live intelligence map** — camera locations (Flock, ALPR, traffic, red
  light, speed), community reports, traffic layer, risk scoring.
- **AI navigation** — route scoring by camera exposure, with preference modes
  (Fastest, Privacy, Calm, Scenic).
- **AI scanner** — camera-based infrastructure detection (CoreML), BLE device
  discovery and classification, live sensor monitoring.
- **Community reporting** — 12 anonymous report types with confidence scoring
  and time decay.
- **Privacy-first** — Ghost Mode, on-device processing, no account, encrypted
  local storage, biometric auth.

Requires Xcode 15+ / iOS 17+ to build.

---

## 📡 The firmware detector (how it works)

This is the microcontroller piece you asked about: a **receive-only** ESP32
scanner that surfaces Flock Cam surveillance devices in the area. It lives in
[`firmware/`](firmware/README.md).

### Why 2.4GHz WiFi, not BLE

Flock cameras historically broadcast a management WiFi access point
(deactivated around December 2025) and BLE maintenance beacons (which stopped
working in spring 2026). The current, community-verified signal is different:
**Flock cameras transmit wildcard 802.11 probe requests**.

A *wildcard probe request* is a WiFi management frame where a client (the
camera, in station mode) announces "is there any access point here?" with an
empty (length-0) SSID. Flock cameras emit these on ascending WiFi channels at
roughly 0.125-second intervals.

Because the radio only *listens*, this is **passive reception** — the device
never transmits, never probes, never associates to a network, and never creates
an access point.

### Detection pipeline

```
 [2.4GHz air]
      │
      ▼
 wifiSniffer() ───── promiscuous-mode IRAM callback (WiFi task)
      │                fast OUI match only; no Serial / no malloc
      ▼
 alertQueue[32] ────  lock-free ring buffer (ISR-safe)
      │
      ▼
 drainAlertQueue() ─ loop() context; fyAddDetection() keeps best tier per MAC
      │
      ├──► tierChirp(tier) + LED flash     (distinct audio per tier)
      ├──► emitDetectionJSON()             (one JSON line over USB/serial)
      └──► dedupe (<5s rate limit; a higher tier preempts)
```

The split between the WiFi-task callback and `loop()` is deliberate: the WiFi
task has hard real-time constraints and must not call `Serial.print` or
`malloc`. The callback writes only to a lock-free ring buffer; `loop()` does the
heavy lifting.

### Matching signals → confidence tiers

A Flock camera is identified by three features combined:

1. **Transmitter MAC OUI** — the first 3 bytes (OUI) of the transmitter's MAC
   belong to the known Flock set.
2. **Wildcard SSID element** — the probe request's SSID tag has length 0.
3. **Information-Element fingerprint** — vendor-specific IEs match the
   DeFlockJoplin signature.

Every observed frame is assigned the **highest-confidence tier** that applies:

| Tier | Detection method | Gate | Confidence |
|------|------------------|------|-----------|
| 4 | `wildcard_probe_ie_sig` | OUI + wildcard SSID + IE fingerprint | high (no observed false positives in drive-testing) |
| 3 | `wildcard_probe` | OUI + wildcard SSID, IE unverified | strong |
| 2 | `oui_addr2` | transmitter-side OUI on any frame | moderate |
| 1 | `oui_addr1` / `oui_addr3` | receiver / BSSID OUI (AP echo) | weaker, noisier |
| 0 | `ssid` | SSID keyword (off by default) | weakest |

Each tier plays a **distinct sound** so the confidence method is obvious by ear
while driving (tier 4 is a high two-note ascending chirp, tier 1 a low single
blip). Detection and logging happen regardless of the sound.

### The 32-prefix OUI target list

The transmitter-MAC set comes from **@NitekryDPaul's** research (31 active
prefixes) plus one OUI contributed by **DeFlockJoplin**:

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

> **Note:** the matcher deliberately does **not** filter out
> locally-administered MACs. `82:6b:f2` has the locally-administered bit set,
> so such a filter would silently drop a real Flock camera.

### Output

Every detection streams as one JSON line over USB/serial (115200 baud):

```json
{"event":"detection","detection_method":"wifi_wildcard_probe_ie_sig","detection_tier":4,"protocol":"wifi_2_4ghz","mac_address":"82:6b:f2:14:07:3a","rssi":-52,"channel":6,"frequency":2437,"ssid":""}
```

While any tier-4 device stays in range, a heartbeat beep repeats every ~10 s.

---

## Getting started (firmware)

Hardware: any ESP32 (ESP32-S3 recommended, e.g. Seeed XIAO ESP32-S3). Buzzer on
GPIO 3, onboard LED on GPIO 21.

```bash
cd firmware
pio run -e xiao_esp32s3 -t upload     # PlatformIO (recommended)
pio device monitor
```

Or prototype on a computer with a monitor-mode WiFi adapter:

```bash
cd firmware
pip install scapy
sudo iw dev wlan0 set type monitor
python3 host_scanner.py --scan 20 --iface wlan0
python3 tests/test_detection.py       # shared OUI/tier unit tests
```

---

## Credits

- **@NitekryDPaul / OrdoOuroboros** — Flock Cam OUI research and the `addr1`
  receiver detection technique.
- **DeFlockJoplin** — the wildcard-probe / Information-Element fingerprint and
  the `82:6b:f2` OUI.
- **colonelpanichacks** — [flock-you](https://github.com/colonelpanichacks/flock-you),
  the firmware whose detection pipeline this project is modeled on.
- **nsm_barii** — observation of Flock camera probe-hop timing.

---

## Legal & ethical

This project performs **passive RF reception only**. The hardware firmware never
transmits, never probes, and never associates to a network; it only observes
802.11 frames its targets broadcast into the air. Detecting the presence of
surveillance hardware in public spaces is legal in most jurisdictions. Always
comply with local laws regarding wireless reception, and use only for authorized
security research and privacy awareness.

*"If it broadcasts, it can be heard."*

## License

MIT — see the `firmware/LICENSE` header.