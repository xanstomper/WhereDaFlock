#!/usr/bin/env python3
"""
WhereDaFlock - tests for the signal math module.

Verifies RSSI→distance, multilateration, and MAC-address-bit analysis without
any hardware. Run:  python3 firmware/tests/test_signal_math.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import signal_math as sm  # noqa


def check(label, got, want):
    ok = got == want
    print(f"  [{'ok ' if ok else 'FAIL'}] {label}")
    return ok


ok = True

print("== distance model ==")
# At the reference RSSI, distance ~1 m.
ok &= check("~1m at TxPower", abs(sm.estimate_distance(-59) - 1.0) < 0.2, True)
# Stronger signal = nearer.
ok &= check("stronger->closer", sm.estimate_distance(-45) < sm.estimate_distance(-75), True)
# Rough label path.
ok &= check("rough label", sm.rssi_to_distance_rough(-60) != "", True)
ok &= check("rssi 0 sentinel", sm.estimate_distance(0) == -1.0, True)

print("\n== MAC address bits ==")
ok &= check("randomized bit set (82..)", sm.is_locally_administered("82:6B:F2:14:07:3A"), True)
ok &= check("universal OUI (70..)", sm.is_locally_administered("70:C9:4E:00:00:01"), False)
ok &= check("multicast (ff)", sm.is_multicast("FF:FF:FF:FF:FF:FF"), True)
ok &= check("unicast (82)", sm.is_multicast("82:6B:F2:14:07:3A"), False)
lab = sm.mac_randomization_label("82:6B:F2:14:07:3A")
ok &= check("label mentions randomized", "randomized" in lab, True)

print("\n== multilateration (synthetic) ==")
# Three anchors on a unit triangle around the origin; solution ~ (1,1).
# Anchor distances chosen to make (1,1) consistent (roughly).
pt = sm.multilaterate_no_numpy([(0, 0, 1.0), (2, 0, 1.0), (1, 2, 1.0)])
ok &= check("returns a fix", pt is not None, True)
ok &= check("x near expected (~1)", pt is None or abs(pt[0] - 1.0) < 0.5, True)
ok &= check("too few anchors -> None", sm.multilaterate_no_numpy([(0, 0, 1), (1, 0, 1)]), None)

print()
print("ALL PASS" if ok else "SOME FAILURES")
sys.exit(0 if ok else 1)