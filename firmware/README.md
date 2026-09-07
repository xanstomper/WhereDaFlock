# WhereDaFlock — Flock Cam Detector Firmware

A pair of **receive-only** detectors for Flock Safety ALPR / edge cameras, both
running on a low-cost ESP32 microcontroller:

- **`WhereDaFlock_scanner.ino`** — 2.4GHz **WiFi promiscuous** sniffer that
  detects Flock wildcard probe requests by OUI + IE fingerprint.
- **`WhereDaFlock_ble.ino`** — **BLE** beacon scanner that detects Flock
  advertisements by their manufacturer Company Identifier **`0x09C8`**.

Each streams detections as NDJSON over USB/serial with optional buzzer + LED
alerts, and each has a host-side Python companion.

> **Tagline:** Navigate freely. Stay unseen.

This `firmware/` folder is the dedicated detector companion to the main
WhereDaFlock app (see the repo root `README.md`).

> ⚠️ **WiFi vs BLE.** Flock cameras historically broadcast a management WiFi AP
> (deactivated ~Dec 2025) and BLE maintenance beacons. Today the primary
> community-verified WiFi signal is **passive probe-request detection** (details
> below), while a confirmed **BLE** method scans for the `0x09C8` manufacturer
> ID. WhereDaFlock ships **both**; BLE and WiFi are complementary —
> running both gives the best coverage.

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
├── WhereDaFlock_ble.ino       # ESP32 BLE beacon scanner (mfr ID 0x09C8)
├── src/
│   ├── signatures.h           # Flock WiFi OUI list + confidence tiers
│   ├── ble_signatures.h       # Flock BLE signatures (0x09C8, names, UUIDs)
│   └── session.h              # SPIFFS persistence + NVS beep mask + host commands
├── host_scanner.py            # WiFi host companion (Scapy)
├── ble_scanner.py             # BLE host companion (Bleak)
├── packet_analyzer.py         # passive frame decoder (educational)
├── tests/
│   ├── test_detection.py      # WiFi OUI/tier tests
│   ├── test_ble_detection.py  # BLE mfr-ID/name/UUID tests
│   └── test_packet_analyzer.py# 802.11 + BLE frame decoder tests
├── platformio.ini             # WiFi env + BLE env targets
├── partitions.csv
└── LICENSE                    # MIT
```

## BLE beacon scanner

`WhereDaFlock_ble.ino` is the BLE detector. It matches the Flock Safety
manufacturer Company Identifier **`0x09C8`** (little-endian first 2 bytes of
the advertisement's manufacturer data) plus advertised-name and service-UUID
patterns, and emits the same NDJSON stream with `protocol:"ble"`.

Build with PlatformIO:

```bash
pio run -e xiao_esp32s3_ble -t upload
```

Host-side BLE scanning (Bleak) + tests:

```bash
python3 ble_scanner.py --scan 20
python3 tests/test_ble_detection.py
```

To validate without a real camera, see
[`tools/emulator/FlockCam_emulator.ino`](../tools/emulator/FlockCam_emulator.ino)
(⚠️ test-only beacon that transmits the Flock mfr ID; bench use only).

## Passive packet analyzer (framing / learning)

`packet_analyzer.py` decodes the actual frames frame-by-frame so you can *see*
why a detection happens — it is the educational companion to the detector,
which only *matches* signatures. It works on WiFi (802.11 probe requests) and
BLE (advertising PDUs), both passively.

Usage (needs `pip install scapy` for capture / offline pcap):

```bash
# Decode an offline capture (no hardware or root needed)
python3 packet_analyzer.py --pcap capture.pcap --count 20

# Live decode on a monitor-mode interface (root)
sudo python3 packet_analyzer.py --live wlan0 --count 5
```

Sample output for a Flock-style probe request:

```
  Frame:  Management / Probe Request  (type=0 subtype=4)
           add2(SA/TA): 82:6B:F2:14:07:3A
  Probe Request body:
        ◄ WILDCARD SSID IE SSID len=0 (wildcard/empty)
           IE Supported Rates len=4 ...
           IE Vendor Specific       ...
  ➜ This is a WILDCARD probe request (any AP may respond).
```

And for a Flock BLE advertisement:

```
  AD type 09 Complete local name 'FS Ext Battery'
  AD type FF Manufacturer Specific  CompanyID=0x09C8 (FLOCK), payload=010203
  ➜ Flock manufacturer Company ID 0x09C8 detected!
```

Regression tests (all passive, no hardware):

```bash
python3 tests/test_packet_analyzer.py
```

## Standalone persistence & device control (WiFi firmware)

Pulled from flock-you's on-device features (`src/session.h`):

- **SPIFFS session persistence** — every 60 s the unique-BY-MAC detection table
  is saved to `/session.json` (atomic tmp→rename). On boot any prior session is
  promoted to `/prev_session.json` so an offline run is preserved intact.
- **NVS per-tier audio mute** — the `wdfBeepMask` (bits 0–4) is stored in NVS
  and survives power cycles. `tierAudible()` honors it.
- **USB-CDC host command channel** — the dashboard (or any host) can send one
  JSON command per line:
  - `{"cmd":"get_config"}` → device re-emits its config JSON
  - `{"cmd":"set_beep","tier":N,"on":0|1}` → mute/unmute one tier
  - `{"cmd":"set_beep_mask","mask":0-31}` → set all tiers at once
  - `{"cmd":"dump_session","source":"live"|"prev"}` → stream the offline table
  - `{"cmd":"clear_session"}` → clear the on-device table

The dashboard (`api/`) exposes these as `/api/watch/*` endpoints and a
per-tier Audio panel in the UI.

---

## Credits

- **@NitekryDPaul / OrdoOuroboros** — Flock Cam OUI research and `addr1`
  detection technique ([nite-oui-collection](https://github.com/nitekry/nite-oui-collection)).
- **DeFlockJoplin** — wildcard-probe and Information-Element fingerprint plus
  the `82:6b:f2` OUI.
- **colonelpanichacks** — [flock-you](https://github.com/colonelpanichacks/flock-you)
  firmware this detection pipeline is modeled on.
- **nsm_barii** — observation of the camera's probe hop timing.
- **@wgreenberg** — Flock Safety BLE manufacturer Company Identifier **`0x09C8`**
  detection research (basis of the `0x09C8` scans in this repo).
- **LuxStatera** — [flock-hunter-cyd-ble](https://github.com/LuxStatera/flock-hunter-cyd-ble),
  the BLE `0x09C8` detector + emulator this path draws from.

---

## Legal & ethical

This tool performs **passive RF reception only**. It never transmits, never
probes, and never associates to a network; it only observes 802.11 management
and data frames Flock cameras broadcast into the air. Detecting the presence of
surveillance hardware in public spaces is legal in most jurisdictions. Always
comply with local laws regarding wireless reception, and use only for
authorized security research and privacy awareness.

*"If it broadcasts, it can be heard."*