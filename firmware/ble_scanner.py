#!/usr/bin/env python3
"""
WhereDaFlock BLE - host-side passive BLE scanner (Beta).

A cross-platform companion to the ESP32 BLE firmware. It scans BLE
advertisements and scores them against the Flock signature set, mirroring
the firmware's logic in `WhereDaFlock_ble.ino` / `src/ble_signatures.h`.

Decisive signal: the Flock Safety manufacturer Company Identifier 0x09C8 in
the advertisement's manufacturer-specific data (the first 2 bytes,
little-endian). Supporting signals: advertised device name and service UUIDs.

PASSIVE ONLY: this tool only listens. It never advertises or connects.

Requires: `pip install bleak`
Usage:
    python3 ble_scanner.py --scan 20
    python3 ble_scanner.py --scan 30 --json results.json
"""

import argparse
import asyncio
import json
import sys
from datetime import datetime, timezone

# ---------------------------------------------------------------------------
# Signatures (mirrors firmware/src/ble_signatures.h)
# ---------------------------------------------------------------------------
FLOCK_MFR_ID = 0x09C8

NAME_PATTERNS = [
    "FLOCK", "FS EXT BATTERY", "FS-CAM", "FLOCK-SAFETY", "PENGUIN", "PIGVISION",
]
SERVICE_UUID_FRAGMENTS = [
    "180a", "180f", "1819", "3100", "3200", "3300", "3400", "3500",
]

MFR_ID_WEIGHT = 70
NAME_MATCH_WEIGHT = 45
SERVICE_UUID_WEIGHT = 20
RSSI_PROXIMITY_BONUS = 5

LIKELY_THRESHOLD = 60
POSSIBLE_THRESHOLD = 30
TX_POWER_1M = -59


def estimate_distance(rssi: int) -> float:
    if rssi == 0:
        return -1.0
    ratio = rssi / TX_POWER_1M
    if ratio < 1.0:
        return ratio ** 10
    return (0.89976 * (ratio ** 7.7095)) + 0.111


def has_flock_mfr_id(manufacturer_data: bytes) -> bool:
    """True if the first 2 bytes of manufacturer data, little-endian, == 0x09C8."""
    if not manufacturer_data or len(manufacturer_data) < 2:
        return False
    mfr_id = manufacturer_data[0] | (manufacturer_data[1] << 8)
    return mfr_id == FLOCK_MFR_ID


def classify(name: str, manufacturer_data: bytes, service_uuids) -> tuple[int, str]:
    """Return (confidence, method) for a single advertisement."""
    score = 0
    method = "none"
    if has_flock_mfr_id(manufacturer_data):
        score += MFR_ID_WEIGHT
        method = "mfr_id"
    name_up = (name or "").upper()
    for pat in NAME_PATTERNS:
        if pat in name_up:
            score += NAME_MATCH_WEIGHT
            if method == "none":
                method = "name"
            break
    uuid_blob = "".join(u.upper() for u in (service_uuids or []))
    for frag in SERVICE_UUID_FRAGMENTS:
        if frag.upper() in uuid_blob:
            score += SERVICE_UUID_WEIGHT
            if method == "none":
                method = "service_uuid"
            break
    return min(100, score), method


def verdict_for(score: int, rssi: int) -> str:
    if rssi >= -50 and score > 0:
        score = min(100, score + RSSI_PROXIMITY_BONUS)
    if score >= LIKELY_THRESHOLD:
        return "FLOCK_LIKELY"
    if score >= POSSIBLE_THRESHOLD:
        return "FLOCK_POSSIBLE"
    return "CANDIDATE"


# ---------------------------------------------------------------------------
# Live scan (Bleak)
# ---------------------------------------------------------------------------
_found = {}
RSSI_THRESHOLD = -90


def _on_discovery(device, advertisement_data):
    try:
        name = device.name or advertisement_data.local_name or ""
    except Exception:
        name = device.name or ""
    mac = getattr(device, "address", None) or getattr(device, "mac", None) or ""
    if not mac:
        return
    rssi = advertisement_data.rssi
    if rssi is None or rssi < RSSI_THRESHOLD:
        return
    mfg = advertisement_data.manufacturer_data
    if not mfg:
        return  # no manufacturer data to inspect
    # Bleak exposes manufacturer_data as {company_id: payload}; take first.
    mfr_raw = None
    for co, payload in mfg.items():
        mfr_raw = payload
        break
    service_uuids = list(getattr(advertisement_data, "service_uuids", []) or [])

    score, method = classify(name, mfr_raw or b"", service_uuids)
    if score == 0:
        return
    verdict = verdict_for(score, rssi)

    key = mac
    entry = {
        "ts": datetime.now(timezone.utc).isoformat(),
        "mac": mac,
        "name": name or "?",
        "rssi": rssi,
        "dist_m": round(estimate_distance(rssi), 2),
        "conf": score,
        "method": method,
        "mfr_id": f"0x{FLOCK_MFR_ID:04X}",
        "verdict": verdict,
        "services": [u for u in service_uuids if u],
    }
    _found[key] = entry
    print(f"[{verdict:<14}] {mac} | {name or '?':<20} | "
          f"RSSI {rssi:>4} dBm | conf {score:>3}% | {method}")


async def run_scan(duration: int):
    from bleak import BleakScanner
    scanner = BleakScanner(detection_callback=_on_discovery)
    print(f"WhereDaFlock BLE host scanner - passive, {duration}s...\n")
    await scanner.start()
    await asyncio.sleep(duration)
    await scanner.stop()


def main():
    ap = argparse.ArgumentParser(description="WhereDaFlock passive BLE scanner")
    ap.add_argument("--scan", type=int, default=20, help="scan duration seconds")
    ap.add_argument("--rssi", type=int, default=-90, help="RSSI floor")
    ap.add_argument("--json", default=None, help="optional JSON export path")
    args = ap.parse_args()

    global RSSI_THRESHOLD
    RSSI_THRESHOLD = args.rssi

    try:
        asyncio.run(run_scan(args.scan))
    except ModuleNotFoundError:
        sys.exit("Install BLE stack first: pip install bleak")

    print(f"\nDone. {len(_found)} matching signature(s) observed.")
    if args.json:
        with open(args.json, "w") as f:
            json.dump(list(_found.values()), f, indent=2)
        print(f"Wrote {args.json}")


if __name__ == "__main__":
    main()