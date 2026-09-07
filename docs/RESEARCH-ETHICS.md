# WhereDaFlock — Research, Ethics & Lawful Learning

This document is the companion to the code: how to study radio detection and
surveillance technology **lawfully**, and the ethical line the project stays
on. WhereDaFlock is a **passive receiver**. Everything below is about learning
and research while remaining on the right side of the law.

> **The one-line rule:** *Listening* to what's broadcast into the air is lawful
> research in most jurisdictions. *Transmitting* to interfere, spoof, or access
> systems you don't own is not. This project only ever listens.

---

## 1. Why this is a research project, not an attack tool

The firmware and host tools are receive-only by design:

```
 [2.4GHz air]
      │
      ▼   (the cameras broadcast these — anyone can receive them)
 WiFi wildcard probes / BLE 0x09C8 advertisements
      │
      ▼   WhereDaFlock only READS these frames
 decode → score → report (JSON over USB)
```

Receiving a WiFi probe request or a BLE advertisement is identical to what your
phone does every time it scans for networks. It involves **no interaction** with
the transmitter: no connection, no authentication, no response. That is the
foundation of lawful passive RF research.

---

## 2. The lawful-use boundary (be explicit)

| Activity | Lawful research? | Why |
|----------|------------------|-----|
| **Passive reception** (detect, decode, log) | ✅ Yes | Same as a phone scanning for Wi-Fi/BLE; FCC/telecom lawful-use criteria |
| Maintaining a **lab-only test beacon** (own bench, own hardware) | ✅ Yes* | *Only on your own isolated test network, clearly labeled, never in public |
| **Jamming / RF interference** | ❌ No | Illegal under FCC §302/§333 and equivalent law in most countries (criminal) |
| **Spoofing / false-data injection** | ❌ No | Interferes with third-party systems; fraud / interference statutes |
| **Unauthorized access** (camera firmware, vendor cloud, ALPR backend) | ❌ No | Unauthorized access / computer-crime statutes |
| Interfering with **emergency/911** adjacent systems | ❌ No | Heightened penalties (these cameras connect to emergency-vehicle alerting) |

If you are unsure whether something falls outside the two middle columns, treat
it as out of scope. "It worked in a lab" is not the same as "lawful to deploy."

---

## 3. Building a lawfully-isolated lab

If you want to experiment deeper, keep it on **your own infrastructure**:

1. **Own hardware, own bench.** Both the detector *and* the thing it detects
   should be devices you own, on a bench or a room you control.
2. **Isolate the RF.** A Faraday bag or a shielded enclosure for the transmitting
   test beacon keeps its signals local. A USB-serial analyser or a second ESP32
   can act as the "target" without leaving your desk.
3. **No broadcast outward.** Never run a test beacon in a public space, out a
   window, or on a network that reaches past your own equipment.
4. **Log only your own traffic.** Capture frames your own devices emit. Don't
   build a standing recorder pointed at neighbors' or the street's traffic.
5. **Label and tear down.** Test rigs are ephemeral. Don't leave transmitting
   fixtures running unattended.

The `tools/emulator/FlockCam_emulator.ino` included here exists **only** for
this bench-validation pattern (prove your detector works before trusting it).
It impersonates the mfr ID on purpose, and it is the *only* transmit-capable
sketch in the repo.

---

## 4. What "learning" looks like here (all lawful)

Legitimate education around this technology:

- **Radio fundamentals** — 802.11 frame format, BLE advertising packets,
  Information Elements, manufacturer Company IDs, RSSI, channel hopping.
  Study against the specs themselves (see §6).
- **Protocol reading** — parse real probe requests / advertisements you
  encounter *passively* to understand structure (this is what the detector does).
- **Detection science** — confidence scoring, false positives, sensor fusion,
  signal-to-noise, triangulation from RSSI.
- **Privacy & counter-surveillance** — the flip side: how everyday RF reveals
  presence, and how to reason about your own exposure.
- **Defensive hardening** — what mitigations exist (MAC randomization, zones,
  policy), and when they matter.

All of these are study, not intrusion.

---

## 5. Applying it to WhereDaFlock

The educational surfaces already in the repo:

| Surface | What to learn |
|---------|---------------|
| `docs/DETECTION-GUIDE.md` | 802.11 Anatomy, IE fingerprint, confidence tiers, false positives |
| `docs/BLE-GUIDE.md` | BLE advertising packet, manufacturer Company ID, decoding |
| `docs/HARDWARE.md` | RF basics, RSSI→distance, antenna behavior |
| `firmware/src/signatures.h` | Curated OUI research + change log (`datasets/NitekryDPaul_wifi_ouis.md`) |
| `firmware/tests/*` | How a detection decision is formed, and how to test it |
| `api/` | Turning raw frames into a dashboard + CSV/KML (data pipeline) |

A good self-study exercise: flash the detector and the emulator **both in a
shielded box**, then inspect the JSON. Change the emulator's advertised name and
watch the confidence method flip from `mfr_id` to `name`. That teaches the
detection logic without any external target.

---

## 6. Authoritative references (read these, don't attack)

These are the legitimate sources this project cites and the right places to
learn the underlying science:

- **802.11 (Wi-Fi) standard** — IEEE 802.11; probe requests, management frames,
  Information Elements.
- **IEEE GLOBECOM 2022** — Pintor & Atzori, *Analysis of Wi-Fi Probe Requests
  Towards Information Element Fingerprinting*.
- **BLE / Bluetooth Core Spec** — Bluetooth SIG; advertising packets and
  Manufacturer-Specific Data / Company ID (0x09C8 belongs to XUNTONG).
- **FCC Part 15** — the US rules on intentional/unintentional emitters, which is
  the legal baseline for passive reception and the reason active interference
  is prohibited.
- Analogous national regulators for wherever you operate.

---

## 7. Summary

- WhereDaFlock stays **passive** — that's what makes it a lawful learning tool.
- Build your experiments on **your own, isolated infrastructure**.
- **Never transmit** to interfere, spoof, or reach systems you don't own.
- When in doubt, study rather than deploy.

*If it broadcasts, it can be heard — but hearing is research, not attack.*