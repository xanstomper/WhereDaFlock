# WhereDaFlock — BLE Detection Guide

How the WhereDaFlock **BLE** scanner detects Flock Safety hardware by their
Bluetooth Low Energy advertisements. This is the companion to
[`DETECTION-GUIDE.md`](DETECTION-GUIDE.md) (the WiFi probe method).

---

## 1. What a Flock cam advertises

BLE devices broadcast advertisement packets periodically. A Flock camera's
advertisement includes a **manufacturer-specific data** field whose first two
bytes are the Bluetooth Company Identifier assigned to the manufacturer.

Flock Safety hardware uses Company Identifier **`0x09C8`**. This identifier is
registered to **XUNTONG** and was identified for this use by **@wgreenberg**.
Because it is a registered, non-random Company Identifier, matching it is a
strong signal.

> **BLE vs WiFi.** The WiFi (probe) method described in `DETECTION-GUIDE.md`
> needs the camera's WiFi radio to be actively probing. BLE is a separate,
> complementary radio. A camera whose WiFi radio is quiet can still be found by
> BLE, and vice-versa. For best coverage run both detectors.

---

## 2. The advertisement anatomy (manufacturer data)

A BLE advertising packet's manufacturer-specific data begins with a 2-byte
Company Identifier stored **little-endian**:

```
 Byte 0  Byte 1  Byte 2 ...   →   payload
 +------+------+--------+
 | 0xC8 | 0x09 | <status bytes...> |
 +------+------+--------+

 company_id = byte0 | (byte1 << 8) = 0x09C8
```

The ESP32 firmware decodes this as:

```c
uint16_t mfrId = (uint8_t)data[0] | ((uint8_t)data[1] << 8);
bool isFlock = (mfrId == 0x09C8);
```

---

## 3. Matching signals → confidence

Three signals, weighted so a single decisive one is enough but weak ones never
false-positive alone:

| Signal | Weight | Notes |
|--------|--------|-------|
| **Manufacturer ID `0x09C8`** | **70** | decisive Flock signal |
| **Advertised name** substring | 45 | `FS EXT BATTERY`, `FLOCK`, `FS-CAM`, `PENGUIN`, `PIGVISION` |
| **Service UUID** fragment | 20 | `180a` (Device Info), `180f` (Battery), `1819` (loc/nav), `31xx–35xx` (companion) |

Verds:

| Score | Verdict | Behavior |
|-------|---------|----------|
| ≥ 60 | `FLOCK_LIKELY` | alarm buzz + LED, always recorded |
| 30–59 | `FLOCK_POSSIBLE` | recorded quietly |
| < 30 | `CANDIDATE` | recorded, low confidence |

The `0x09C8` ID alone scores 70 → `FLOCK_LIKELY` with no other signal needed.
Name-only matches (e.g. an "FS Ext Battery" with no MFR data) score 45 → still
worth reporting, but below the loud-alarm threshold alone.

---

## 4. Firmware (`WhereDaFlock_ble.cpp`)

- Uses the ESP32 BLE scan API (`BLEScan`), **active scan** to request scan
  responses (which can reveal the readable name), live processing (`setMaxResults(0)`).
- Decisive match: `hasFlockMfrId()`.
- Secondary: `matchesName()` and `matchesServiceUUID()`.
- Emits NDJSON with `protocol:"ble"`, `method:"mfr_id"|"name"|"service_uuid"`.
- Buzzer + LED on `FLOCK_LIKELY`; heartbeat re-beep while in range.

Build:

```bash
cd firmware
pio run -e xiao_esp32s3_ble -t upload     # or the Arduino CLI command below
```

Arduino CLI:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 WhereDaFlock_ble.cpp
arduino-cli upload --fqbn esp32:esp32:esp32 --port /dev/ttyUSB0 WhereDaFlock_ble.cpp
```

Output:

```json
{"event":"new","protocol":"ble","mac":"D4:E9:F4:B1:40:0C","name":"FS Ext Battery","rssi":-62,"dist_m":1.4,"conf":70,"method":"mfr_id","mfr_id":"0x09C8","verdict":"FLOCK_LIKELY"}
```

---

## 5. Host-side BLE (`ble_scanner.py`)

Mirrors the firmware logic over a computer's Bluetooth adapter (Bleak):

```bash
pip install bleak
python3 ble_scanner.py --scan 20
python3 ble_scanner.py --scan 30 --json results.json
```

Unit tests (no hardware needed):

```bash
python3 firmware/tests/test_ble_detection.py
```

---

## 6. Validating without a real camera

Flash [`tools/emulator/FlockCam_emulator.ino`](../tools/emulator/FlockCam_emulator.ino)
onto a **second** ESP32. It advertises a Flock-like name ("FS Ext Battery") and
manufacturer data beginning `0xC8 0x09` (the `0x09C8` Company Identifier), so
your detector should report a `FLOCK_LIKELY` `mfr_id` hit within a scan window.

> ⚠️ The emulator **transmits** a spoofed Flock Manufacturer ID. It is a
> lab validation fixture only — never leave it running in public.

---

## 7. Tuning / extending

- Add/remove name patterns in `firmware/src/ble_signatures.h`
  (`NAME_PATTERNS[]`) — mirror in `firmware/ble_scanner.py` and add a test.
- Adjust `MFR_ID_WEIGHT`, `NAME_MATCH_WEIGHT`, `SERVICE_UUID_WEIGHT` there.
- Raise/lower `LIKELY_THRESHOLD` / `POSSIBLE_THRESHOLD` to trade sensitivity
  against false positives in your environment.

Consistency rule: `src/ble_signatures.h`, `ble_scanner.py`, and the tests must
stay in sync (same verifiable pattern as the WiFi OUI list).

---

## 8. References

- **@wgreenberg** — Flock `0x09C8` BLE manufacturer-ID research.
- **LuxStatera** — [flock-hunter-cyd-ble](https://github.com/LuxStatera/flock-hunter-cyd-ble),
  the BLE detector + emulator this path draws from.
- **XUNTONG** — the BLE Company Identifier administrator for `0x09C8`.