# WhereDaFlock — Build & Flash Guide

Complete instructions for building and flashing the WhereDaFlock ESP32 firmware,
plus hardware selection and troubleshooting.

---

## 1. Prerequisites

- **Board:** any ESP32 with a 2.4GHz WiFi radio. Recommended: **Seeed XIAO
  ESP32-S3** (default target in `platformio.ini`). A generic ESP32-S3 DevKitC
  also works (see §4 for the alternate env).
- **Build tool:** PlatformIO (recommended) or the Arduino CLI.
- **8 MB flash board preferred.** The default partition layout (see
  `partitions.csv`) expects 8 MB flash with a 6 MB app region. If your board has
  only 4 MB, use a smaller SPIFFS partition (see §6).

### Quick check — is my board supported?

```bash
# XIAO ESP32-S3
pio board seeed_xiao_esp32s3 2>/dev/null | head -3
```

---

## 2. Build with PlatformIO (recommended)

```bash
cd firmware

# 1. Install the environment's toolchain (first run only)
pio run -e xiao_esp32s3

# 2. Build
pio run -e xiao_esp32s3

# 3. Flash over USB
pio run -e xiao_esp32s3 -t upload

# 4. Watch detections (JSON lines @ 115200)
pio device monitor
```

> PlatformIO downloads its own toolchain and core; you do **not** need the
> Arduino IDE or `arduino-cli` installed separately.

---

## 3. Build with Arduino CLI

If you prefer the Arduino toolchain:

```bash
# 1. Install the ESP32 core (RISC-V toolchain too, if your board needs it)
arduino-cli core install esp32:esp32

# 2. Compile for the ESP32-S3
arduino-cli compile --fqbn esp32:esp32:esp32s3 WhereDaFlock_scanner.ino

# 3. Upload
arduino-cli upload --fqbn esp32:esp32:esp32s3 --port /dev/ttyUSB0 WhereDaFlock_scanner.ino

# 4. Monitor
arduino-cli monitor --port /dev/ttyUSB0 --config baudrate=115200
```

> The firmware uses the `WiFi` + `esp_wifi` promiscuous APIs that ship with
> the ESP32 Arduino core. No third-party library is required.

---

## 4. Pin configuration

The firmware uses `#ifndef` guards so you can override pins from your build
system without editing source:

| Define | Default | Board |
|--------|---------|-------|
| `BUZZER_PIN` | `3` | XIAO ESP32-S3 |
| `LED_PIN` | `21` | XIAO ESP32-S3 onboard (active low) |
| `LED_ACTIVE_HIGH` | `0` | `0` = active low, `1` = active high |

Override via PlatformIO:

```ini
[env:my_board]
board = esp32-s3-devkitm-1
build_flags =
    -D BUZZER_PIN=18
    -D LED_PIN=2
    -D LED_ACTIVE_HIGH=1
```

### Wiring (XIAO ESP32-S3, default)

```
XIAO GPIO 3  ──►  Piezo buzzer (+)      (via ~100 Ω series resistor)
XIAO GND      ──►  Piezo buzzer (-)
XIAO GPIO 21  ──►  Onboard LED  (active low; no external wiring needed)
USB           ──►  Host @115200 baud (JSON output)
```

| Signal | Value |
|--------|-------|
| Serial bitrate | 115200 |
| Channel order | 11 → 6 → 1 (descending, against camera's ascending) |
| Channel dwell | 250 ms |
| RSSI floor | −95 dBm |
| Detection rate-limit | 5 s per MAC |
| Heartbeat | every 30 s while a tier-4 device is in range |

---

## 5. What you should see

On boot, the firmware prints:

```
WhereDaFlock v2.0.0 - passive 2.4GHz Flock Cam detector
RECEIVE-ONLY promiscuous mode. No transmissions.
Targeting 32 Flock OUIs (Sep  7 2026)
Scanning channels 11/6/1 ...
```

Then one JSON line per detection:

```json
{"event":"detection","detection_method":"wifi_wildcard_probe_ie_sig","detection_tier":4,"protocol":"wifi_2_4ghz","mac_address":"82:6b:f2:14:07:3a","rssi":-52,"channel":6,"frequency":2437,"ssid":""}
```

If you never see a match, your board may be on a channel the camera isn't using,
or there are simply no Flock Cameras in range (see troublingshooting below).

---

## 6. Troubleshooting

### "No platform found" / build fails to install RISC-V toolchain

Some ESP32 cores try to install the `riscv32-esp-elf` toolchain even for
`xtensa`-based flash. With PlatformIO this is handled automatically. With
`arduino-cli`, if the RISC-V tool extraction fails, you can still target an
`xtensa` board by core, but the CLI may abort. Prefer PlatformIO in that case.

### Board has 4 MB flash, not 8 MB

Reduce the SPIFFS region. Example 4 MB layout:

```
# Name,   Type, SubType, Offset,   Size,     Flags
nvs,      data, nvs,     0x9000,   20K,
otadata,  data, ota,     0xe000,   8K,
app0,     app,  ota_0,   0x10000,  3M,
app1,     app,  ota_1,   0x310000, 3M,
spiffs,   data, spiffs,  0x610000, 1500K,
```

Recompile with `board_build.partitions = my_4mb.csv` in `platformio.ini`.

### Detections show but only tier 1–2 (no chirp)

That's expected behavior. Tier 1 (AP echo) and tier 2 (OUI on any frame) are
intentionally noisier paths. Hold a consistent position for a dwell cycle so a
wildcard probe + IE fingerprint (tier 4) can register.

### "Wrong boot mode" / upload hangs

Enter download mode by holding the **BOOT** button while power-cycling, then
release after the upload starts. On XIAO ESP32-S3 there is a BOOT pad some
boards require shorting.

### Nothing at all on serial

1. Confirm the port and baud (`pio device monitor -b 115200`).
2. Confirm the board is in the right mode.
3. Confirm you built for the correct FQBN (`esp32s3` vs `esp32`).

---

## 7. Updating the OUI/signature set

See [`DETECTION-GUIDE.md`](DETECTION-GUIDE.md) §7. After editing
`src/signatures.h` (and mirroring in `host_scanner.py` + tests), rebuild and
reflash just the app:

```bash
pio run -e xiao_esp32s3 -t upload
```