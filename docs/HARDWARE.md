# WhereDaFlock — Hardware Guide

Everything you need to spec, assemble, and deploy the WhereDaFlock ESP32
detector.

---

## 1. Bill of materials (BOM)

### Minimum viable build

| Item | Example | Notes | Est. cost |
|------|---------|-------|-----------|
| ESP32 board | Seeed XIAO ESP32-S3 | 2.4GHz radio, USB-C, small | $10–15 |
| Piezo buzzer | 5 V active/passive piezo | driven on GPIO 3 | $1–3 |
| Protoboard / cables | breadboard + dupont | — | $2 |
| USB-C cable | data cable (not charge-only) | USB CDC serial | $2 |

### Extended build (recommended for vehicle/wardriving use)

| Item | Example | Notes |
|------|---------|-------|
| Directional / high-gain antenna | 2.4 GHz 5 dBi or panel | improves range (see §4) |
| LiPo + TP4056 charge module | 18650 + TP4056 | for mobile power |
| USB GPS puck (host-side) | Ublox NEO-6M over USB | for time/position tagging |
| 3D-printed/shrink-tube enclosure | — | weather + tamper protection |

---

## 2. Board selection

| Board | Flash | Why / when |
|-------|-------|-----------|
| **Seeed XIAO ESP32-S3** | 8 MB | Default target. Tiny, USB-C, good antenna. |
| ESP32-S3 DevKitC-1 | 8 MB | Breadboard-friendly, through-hole pins. |
| ESP32 (original) | 4 MB | Works, but only 4 MB flash; use a smaller SPIFFS part. |
| ESP32-C3 | 4 MB | Cheapest; RISC-V core. Use a smaller SPIFFS part. |

> The firmware needs only ~64–128 KB of flash for the app itself; the SPIFFS
> region is for the optional detection-table persistence.

---

## 3. Wiring diagram

### 3.1 XIAO ESP32-S3 (default pins)

```
   ┌─── XIAO ESP32-S3 ────┐
   │                      │
   │ GPIO 3 ─────────────────►  Piezo (+) ──► [100 Ω] ──► GND
   │ GPIO 21 (onboard LED)│  (active low — pull to GND to light)
   │ USB-C ───────────────────►  Host (115200 baud, JSON)
   │ GND ────────────────►  common ground rail
   └──────────────────────┘
```

### 3.2 Battery power (optional, for portability)

```
LiPo (+) ──► TP4056 B+ / OUT+
LiPo (-) ──► TP4056 B- / OUT-
TP4056 5V ──► XIAO 5V pad (or 3V3 for 3.7V cell via regulator)
TP4056 GND ─► XIAO GND
```

> Use the XIAO **5V** pin when a USB charge board feeds it, or a regulated
> 3.3 V output if the cell connects directly. Do not exceed 5.5 V on the 5V pin.

---

## 4. Antenna guidance

- The XIAO ESP32-S3 onboard PCB antenna is omni-directional and adequate for
  short-range (a few meters). For wardriving, connect an external 2.4 GHz
  antenna with an **IPEX/u.FL connector** if your board exposes one.
- A **directional antenna** (e.g. 5–8 dBi panel) improves detection distance
  in one direction but reduces coverage behind it.
- Keep the radio away from large metal objects and animal enclosures; metal
  degrades RSSI and therefore tier confidence/proximity.

### RSSI → approximate distance (for a 2.4 GHz beacon)

| RSSI (dBm) | Approx. range |
|------------|---------------|
| −30 to −50 | very close (< 2 m) |
| −50 to −70 | close (2–10 m) |
| −70 to −85 | medium (10–30 m) |
| −85 to −95 | far (> 30 m, at the RSSI floor) |

> RSSI is affected by obstacles, interference, and antenna orientation. Use it
> as a rough proximity indicator only.

---

## 5. Enclosure & mounting

- Use a **ventilated** enclosure if running outdoors; the ESP32 and (optionally)
  a GPS puck can heat up.
- Route the buzzer so its sound is audible through the case (drill a small port
  or use a sealed piezo with a membrane).
- If deploying on a vehicle, zip-tie or screw-mount the board flat so the PCB
  antenna points upward for best omni coverage.

---

## 6. Power & runtime expectations

- **USB powered:** continuous operation; no battery sizing needed.
- **LiPo (18650 ≈ 2500 mAh):** an ESP32-S3 in promiscuous-sniff (lightweight)
  mode draws roughly 40–80 mA average. Expect **~30–60 h** from a single cell;
  a 5 Ah cell gives several days. Confirm with your own current measurement.

---

## 7. Host-side companion hardware

The `host_scanner.py` Python companion runs on a computer with a **monitor-mode
WiFi adapter**. Not every adapter supports monitor mode. Confirm before buying:

```bash
iw list 2>/dev/null | grep -i monitor   # Linux; check supported modes
```

Commonly compatible chips: **Atheros AR9271**, **Realtek RTL8812AU**,
**Intel AX200/210** (monitor is supported in newer kernels). Avoid chips that
only expose managed mode. See [`PROTOCOL.md`](PROTOCOL.md) for how the host tool
consumes frames.