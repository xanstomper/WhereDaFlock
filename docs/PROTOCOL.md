# WhereDaFlock — Output Protocol & Host Companion

Defines the wire format the ESP32 emits over USB/serial, the human/machine
readable schema, and how `host_scanner.py` mirrors the same logic on a computer.

---

## 1. Serial protocol (ESP32 → host)

The firmware prints **one JSON object per line** at 115200 baud. A consumer can
read the stream continuously; there is no framing beyond newline-delimited JSON
(NDJSON).

### 1.1 Boot line

```json
{"event":"boot","fw":"WhereDaFlock","version":"2.0.0","mode":"promiscuous","channels":"11,6,1","ouis":32}
```

### 1.2 Detection line

```json
{
  "event": "detection",
  "detection_method": "wifi_wildcard_probe_ie_sig",
  "detection_tier": 4,
  "protocol": "wifi_2_4ghz",
  "mac_address": "82:6b:f2:14:07:3a",
  "oui": "82:6b:f2",
  "device_name": "",
  "rssi": -52,
  "channel": 6,
  "frequency": 2437,
  "ssid": ""
}
```

| Field | Type | Meaning |
|-------|------|---------|
| `event` | `string` | always `"detection"` |
| `detection_method` | `string` | the **highest-confidence** path that matched |
| `detection_tier` | `int` | 0–4 confidence tier |
| `protocol` | `string` | `wifi_2_4ghz` |
| `mac_address` | `string` | mac address whose OUI matched |
| `oui` | `string` | first 3 bytes (OUI) of `mac_address` |
| `device_name` | `string` | reserved (WiFi doesn't carry it; always empty) |
| `rssi` | `int` | signal strength in dBm |
| `channel` | `int` | WiFi channel the frame was seen on |
| `frequency` | `int` | center frequency in MHz (`2407 + 5*channel`) |
| `ssid` | `string` | SSID associated with the hit (usually empty for wildcard) |

### 1.3 `detection_method` values

| Method | Tier |
|--------|------|
| `wifi_wildcard_probe_ie_sig` | 4 |
| `wifi_wildcard_probe` | 3 |
| `wifi_oui_addr2` | 2 |
| `wifi_oui_addr1` / `wifi_oui_addr3` | 1 |
| `wifi_ssid` | 0 |

### 1.4 Wireless BLE GATT Telemetry Protocol

When `USE_BLE_TELEMETRY` is enabled on the ESP32 dongle, real-time detection lines are broadcast over Bluetooth Low Energy to any paired mobile companion:

* **Primary Service UUID:** `96F10C00-6DF1-4C00-8000-00805F9B34FB`
* **Telemetry TX Characteristic:** `96F10C01-6DF1-4C00-8000-00805F9B34FB` (Properties: `NOTIFY` | `READ`)
* **Payload Format:** UTF-8 encoded NDJSON matching the serial detection line schema.
* **Subscription:** The iOS companion subscribes via Client Characteristic Configuration Descriptor (CCCD `0x2902`).

---

## 2. Heartbeat

While at least one tier-4 device is in range, a heartbeat beep repeats every
~10 s. Logging also prints a periodic status line:

```
[wdf] scanning ch=6 det=3
```

`det=3` is the number of unique MACs currently in the on-device table.

---

## 3. Host companion (`host_scanner.py`)

The Python companion implements the **same** tier logic so you can prototype
and verify signatures on a normal computer before flashing hardware — or run a
larger scan with full packet capture.

### 3.1 Requirements

- A **monitor-mode capable** WiFi adapter.
- Python 3.8+ and `scapy`.

```bash
pip install scapy
sudo iw dev wlan0 set type monitor    # put adapter in monitor mode
```

### 3.2 Usage

```bash
python3 host_scanner.py --scan 20 --iface wlan0
python3 host_scanner.py --scan 30 --json results.json
```

Arguments:

| Flag | Default | Meaning |
|------|---------|---------|
| `--scan` | `20` | scan duration in seconds |
| `--iface` | `wlan0` | monitor-mode interface |
| `--json` | (none) | write detections to a JSON file |

### 3.3 Live output

```
WhereDaFlock host scanner - passive, 20s on wlan0 ...
[tier 4] 82:6b:f2:14:07:3a  RSSI  -52 dBm  method=wifi_wildcard_probe_ie_sig
```

### 3.4 JSON export schema

```json
[
  {
    "ts": "2026-09-07T00:10:00Z",
    "mac": "82:6b:f2:14:07:3a",
    "rssi": -52,
    "tier": 4,
    "method": "wifi_wildcard_probe_ie_sig",
    "channel": 6
  }
]
```

---

## 4. Consistency between firmware and host tool

Both implementations share:

- The exact same 32-entry OUI list (verified byte-for-byte by the unit test).
- The same tier assignment rules (`OUI+wildcard+IE` = 4, `OUI+wildcard` = 3,
  `OUI any frame` = 2, `addr1/addr3 echo` = 1, `ssid` = 0).
- The same channel-order optimization (descending hops vs the camera's
  ascending hops).

The source of truth for signatures is `firmware/src/signatures.h`; the host
tool must be kept in sync manually (documented in `DETECTION-GUIDE.md` §7).

---

## 5. Device control channel (dashboard → ESP32)

The firmware reads one JSON command per line on the same USB CDC link over
which detections stream. Supported commands (see `firmware/src/session.h` and
`WhereDaFlock_scanner.ino`):

| Command | Effect |
|---------|--------|
| `{"cmd":"get_config"}` | device re-emits its `{"event":"config",...}` line |
| `{"cmd":"set_beep","tier":N,"on":0|1}` | mute/unmute one tier's buzzer |
| `{"cmd":"set_beep_mask","mask":0-31}` | set all five tiers at once |
| `{"cmd":"dump_session","source":"live"|"prev"}` | stream the offline session table |
| `{"cmd":"clear_session"}` | clear the on-device table |

The dashboard exposes these as `GET /api/watch/config`, `POST /api/watch/beep`,
and `POST /api/watch/dump_session`.

A `dump_session` reply is a replay of stored detections with no wall-clock time
(no RTC on the ESP32), so the dashboard ingests them as `source:"replay"` and
does **not** GPS-temporal-match them. Live hits carry `protocol` of
`wifi_2_4ghz` / `ble`; replays (`event: session_det`) do not.

A demo/smoke tester that POSTs synthetic WiFi + BLE detections is at
`api/seed.py` (run it against a live dashboard with no ESP32 needed):

```bash
python3 api/seed.py --count 10
```

---

## 6. Consuming the stream (example)

A trivial Python consumer for the ESP32 serial output:

```python
import serial, json

with serial.Serial('/dev/ttyACM0', 115200, timeout=1) as ser:
    for line in ser:
        line = line.strip()
        if not line:
            continue
        try:
            d = json.loads(line)
        except ValueError:
            continue
        if d.get('event') == 'detection' and d.get('detection_tier', 0) >= 3:
            print(f"{d['mac_address']} -> {d['detection_method']}")
```

---

## 7. Legal note on capture

`host_scanner.py` and the firmware only **listen**. Monitor-mode capture of
2.4GHz management frames is passive. Follow local RF laws; some jurisdictions
restrict even passive capture on devices you do not own, so confirm local rules
before field use.