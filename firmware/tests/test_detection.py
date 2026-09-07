#!/usr/bin/env python3
"""
WhereDaFlock - unit tests for the shared detection / confidence logic.

Verifies the OUI matching and confidence-tier assignment used by both the
ESP32 firmware (port of the same logic) and the host-side scanner.

Run:  python3 test_detection.py
"""

import sys
sys.path.insert(0, ".")
from host_scanner import (
    classify, is_target_oui, tier_of,
    TIER_ECHO, TIER_OUI, TIER_PROBE, TIER_IE_SIG, TIER_SSID,
)


def check(label, got, want):
    status = "ok " if got == want else "FAIL"
    print(f"  [{status}] {label}: got={got!r} want={want!r}")
    return got == want


ok = True

# 1. A known Flock OUI with a wildcard probe + vendor IE -> tier 4.
tier, method = classify("82:6b:f2:14:07:3a", 0, 4, {"ssid": b"", "vendor": b"\x01"},
                        addr1=None, addr3=None, addr_from="82:6b:f2:14:07:3a")
ok &= check("OUI+wildcard+vendor IE tier4", tier == TIER_IE_SIG, True)
ok &= check("tier4 method", method, "wildcard_probe_ie_sig")

# 2. Known OUI + wildcard probe, no vendor IE -> tier 3.
tier, method = classify("70:c9:4e:00:00:01", 0, 4, {"ssid": b"", "vendor": b""},
                        addr_from="70:c9:4e:00:00:01")
ok &= check("OUI+wildcard probe tier3", tier == TIER_PROBE, True)
ok &= check("tier3 method", method, "wildcard_probe")

# 3. Known OUI on any other frame (data/generic mgmt) -> tier 2.
tier, method = classify("3c:91:80:aa:bb:cc", 2, 3, {}, addr_from="3c:91:80:aa:bb:cc")
ok &= check("OUI any-frame tier2", tier == TIER_OUI, True)

# 4. Unknown transmitter OUI but known receiver OUI -> tier 1 echo.
tier, method = classify("f8:ff:c2:11:22:33", 2, 3, {}, addr1="58:00:e3:99:88:77")
ok &= check("addr1 echo tier1", tier == TIER_ECHO, True)

# 5. Completely unknown device -> no match.
tier, method = classify("f8:ff:c2:aa:bb:cc", 0, 4, {"ssid": b"", "vendor": b""},
                        addr1="aa:bb:cc:00:00:00", addr_from="f8:ff:c2:aa:bb:cc")
ok &= check("unknown device no match", tier == -1, True)

# 6. OUI list contains the DeFlockJoplin 82:6b:f2 entry (no locally-admin filter).
ok &= check("82:6b:f2 present", is_target_oui("82:6b:f2:01:02:03"), True)

# 7. First OUI in the list matches.
ok &= check("first OUI present", is_target_oui("70:c9:4e:00:00:00"), True)

# 8. Arbitrary consumer MAC not flagged.
ok &= check("random consumer no OUI", is_target_oui("f8:ff:c2:11:22:33"), False)

print()
print("ALL PASS" if ok else "SOME FAILURES")
sys.exit(0 if ok else 1)