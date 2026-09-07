#!/usr/bin/env python3
"""
WhereDaFlock BLE - unit tests for the shared BLE detection / confidence logic.

Verifies the Flock manufacturer-ID (0x09C8), name, and service-UUID matching
used by both the ESP32 BLE firmware and the host-side Bleak scanner.

Run:  python3 firmware/tests/test_ble_detection.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from ble_scanner import (
    classify, has_flock_mfr_id, verdict_for,
    MFR_ID_WEIGHT, NAME_MATCH_WEIGHT, SERVICE_UUID_WEIGHT,
    LIKELY_THRESHOLD, POSSIBLE_THRESHOLD,
)


def check(label, got, want):
    status = "ok " if got == want else "FAIL"
    print(f"  [{status}] {label}: got={got!r} want={want!r}")
    return got == want


ok = True

# 1. Manufacturer data starting with little-endian 0x09C8 (0xC8 0x09) = Flock.
ok &= check("0x09C8 confirmed", has_flock_mfr_id(bytes([0xC8, 0x09, 0x01])), True)
ok &= check("not 0x09C8 rejected", has_flock_mfr_id(bytes([0x00, 0x18])), False)
ok &= check("too short rejected", has_flock_mfr_id(b"\x00"), False)
ok &= check("empty rejected", has_flock_mfr_id(b""), False)

# 2. MFR ID alone (no name/no UUID) should cross the LIKELY threshold.
score, method = classify("SomeRandomDevice", bytes([0xC8, 0x09, 0x00]), [])
ok &= check("mfr-only score >= likely", score >= LIKELY_THRESHOLD, True)
ok &= check("mfr-only method", method, "mfr_id")

# 3. A name match without MFR ID (e.g. extended battery) is strong but lower.
score, method = classify("FS Ext Battery", b"", ["180f"])
ok &= check("name+uuid score", score == NAME_MATCH_WEIGHT + SERVICE_UUID_WEIGHT, True)
ok &= check("name-first method", method, "name")

# 4. Verdict bucketing.
ok &= check("likely verdict", verdict_for(LIKELY_THRESHOLD, -70), "FLOCK_LIKELY")
ok &= check("possible verdict", verdict_for(LIKELY_THRESHOLD - 1, -70), "FLOCK_POSSIBLE")
ok &= check("candidate verdict", verdict_for(LIKELY_THRESHOLD - 40, -70), "CANDIDATE")

# 5. Proximity bonus only lifts when a signal is present.
ok &= check("strong-signal bonus applied",
            verdict_for(60, -45) == "FLOCK_LIKELY", True)

# 6. Unrelated device (no MFR, no name, no UUID) -> score 0.
score, method = classify("iPhone", b"\x00\x18\x00", ["1800"])
ok &= check("unrelated score 0", score == 0, True)

print()
print("ALL PASS" if ok else "SOME FAILURES")
sys.exit(0 if ok else 1)