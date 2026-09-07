# WhereDaFlock — Flock Cam Passive 2.4GHz Detector

A **receive-only** detector for Flock Safety ALPR / edge cameras that runs on a
low-cost ESP32 microcontroller. It sniffs the 2.4GHz WiFi spectrum for the
wildcard probe requests Flock cameras transmit, matches their transmitter MAC
against the known Flock OUI set, and assigns a confidence tier. Detections
stream as JSON over USB/serial, with optional buzzer + LED alerts.

> **Tagline:** Navigate freely. Stay unseen.

This `firmware/` folder is the dedicated detector companion to the main
WhereDaFlock app (see the repo root `README.md`).

> ⚠️ **Why 2.4GHz WiFi and not BLE?** Flock cameras historically broadcast a
> management WiFi AP (deactivated ~Dec 2025) and BLE maintenance beacons
> (stopped working spring 2026). The current, community-verified method is
> **passive WiFi probe-request detection**, detailed below. This firmware
> targets that method.

---

## How detection works

Flock cameras transmit **wildcard 802.11 probe requests** (SSID tag with length
0) on ascending channels at roughly 0.125 s intervals. These are passively
identifiable by:

| Signal | Tier | Confidence |
|--------|------|-----------|
| **OUI + wildcard SSID + vendor IE fingerprint** | **4** | high (no observed false positives in drive-testing) |
| **OUI + wildcard SSID probe** (IE unverified) | **3** | strong |
| **Transmitter-side OUI** on any frame (`addr2`) | **2** | moderate |
| **Receiver / BSSID OUI** (`addr1` / `addr3`) | **1** | weaker (AP echo, noisier) |
| **SSID keyword** match (off by default) | **0** | weakest |

The 32-prefix OUI target list comes from **@NitekryDPaul's** research
([nite-oui-collection](https://github.com/nitekry/nite-oui-collection)) plus one
OUI contributed by **DeFlockJoplin**. It includes `82:6b:f2`, which has a
locally-administered bit set, so the matcher deliberately avoids a
locally-administered MAC filter.

---

## Hardware

- Any **ESP32** board (ESP32-S3 recommended; the radio runs in promiscuous
  mode). Default pins target the **Seeed XIAO ESP32-S3**.
- Optional piezo buzzer on GPIO 3, onboard LED on GPIO 21 (active low).
- No AP and no station mode are created — the radio spends all its time
  sniffing.

### Wiring (XIAO ESP32-S3 defaults)

```
GPIO  3  ──────►  Piezo buzzer (+)
GND    ──────►  Buzzer (-)
GPIO 21  ──────►  Onboard LED (active low)
USB CDC  ──────►  Host @115200 baud (JSON)
```

---

## Build & flash (PlatformIO)

```
cd firmware
pio run -e xiao_esp32s3              # build
pio run -e xiao_esp32s3 -t upload    # flash
pio device monitor
```

### ...or Arduino CLI

```bash
arduino-cli compile --fqbn esp32:esp32:esp32s3 WhereDaFlock_scanner.ino
arduino-cli upload --fqbn esp32:esp32:esp32s3 --port /dev/ttyUSB0 WhereDaFlock_scanner.ino
arduino-cli monitor --port /dev/ttyUSB0 --config baudrate=115200
```

`platformio.ini` / `partitions.csv` live at the repo root if you use the full
project; for a bare firmware build you can create a minimal `platformio.ini`
targeting `xiao_esp32s3` with a 6 MB app / 1.94 MB SPIFFS partition.

---

## Serial output

Each detection emits one JSON line:

```json
{"event":"detection","detection_method":"wifi_wildcard_probe_ie_sig","detection_tier":4,"protocol":"wifi_2_4ghz","mac_address":"82:6b:f2:14:07:3a","rssi":-52,"channel":6,"frequency":2437,"ssid":""}
```

Per-tier audible cues (all on by default, mask bit = tier):

| Tier | Method suffix | Sound |
|------|---------------|-------|
| 4 | `wildcard_probe_ie_sig` | two-note ascending 2000→2800 Hz |
| 3 | `wildcard_probe` | two-note ascending 1400→1800 Hz |
| 2 | `oui_addr2` | single blip 1200 Hz |
| 1 | `oui_addr1_addr3` | single blip 800 Hz |
| 0 | `ssid` | single blip 600 Hz |

---

## Host-side companion (prototype on a computer)

The same logic is ported to `host_scanner.py` for prototyping with a
monitor-mode WiFi adapter and Scapy:

```bash
sudo pip install scapy
sudo iw dev wlan0 set type monitor        # put your adapter in monitor mode
python3 host_scanner.py --scan 20 --iface wlan0
python3 host_scanner.py --scan 30 --json results.json
```

Run the unit tests (shared matching / tier logic):

```bash
cd firmware
python3 tests/test_detection.py
```

---

## Project layout

```
firmware/
├── WhereDaFlock_scanner.ino   # ESP32 WiFi promiscuous detector (main firmware)
├── src/
│   └── signatures.h           # Flock OUI list + confidence tiers
├── host_scanner.py            # cross-platform Python companion (Scapy)
├── tests/
│   └── test_detection.py      # tests for the shared OUI/tier logic
└── LICENSE                    # MIT
```

---

## Credits

- **@NitekryDPaul / OrdoOuroboros** — Flock Cam OUI research and `addr1`
  detection technique ([nite-oui-collection](https://github.com/nitekry/nite-oui-collection)).
- **DeFlockJoplin** — wildcard-probe and Information-Element fingerprint plus
  the `82:6b:f2` OUI.
- **colonelpanichacks** — [flock-you](https://github.com/colonelpanichacks/flock-you)
  firmware this detection pipeline is modeled on.
- **nsm_barii** — observation of the camera's probe hop timing.

---

## Legal & ethical

This tool performs **passive RF reception only**. It never transmits, never
probes, and never associates to a network; it only observes 802.11 management
and data frames Flock cameras broadcast into the air. Detecting the presence of
surveillance hardware in public spaces is legal in most jurisdictions. Always
comply with local laws regarding wireless reception, and use only for
authorized security research and privacy awareness.

*"If it broadcasts, it can be heard."*