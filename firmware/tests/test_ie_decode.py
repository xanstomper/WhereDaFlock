#!/usr/bin/env python3
"""
WhereDaFlock - tests for the deep 802.11 IE decoder.

Run:  python3 firmware/tests/test_ie_decode.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import ie_decode as ies  # noqa


def check(label, got, want):
    ok = got == want
    print(f"  [{'ok ' if ok else 'FAIL'}] {label}: got={got!r}")
    return ok


BE = lambda n: n.to_bytes(2, "big")
ok = True

print("== RSN (WPA2) ==")
rsn = (BE(1) + bytes([0x00, 0x0F, 0xAC, 0x04]) +    # version, group CCMP
       BE(1) + bytes([0x00, 0x0F, 0xAC, 0x04]) +     # 1x pairwise CCMP
       BE(1) + bytes([0x00, 0x0F, 0xAC, 0x02]) +     # 1x akm PSK
       bytes([0, 0]))
r = ies.decode_rsn(rsn)
ok &= check("ver=1", "ver=1" in r, True)
ok &= check("group CCMP", "CCMP" in r, True)
ok &= check("akm PSK", "PSK" in r, True)

print("== HT / HT Operation ==")
ok &= check("HT 20/40", "20/40MHz" in ies.decode_ht_caps(bytes([0x01, 0x00, 0, 0, 0, 0, 0, 0])), True)
ok &= check("HT op ch6", "primary_ch=6" in ies.decode_ht_info(bytes([6, 0, 0, 0, 0])), True)

print("== Country / VHT / rates ==")
ok &= check("country US", ies.decode_country(b"US\x00").startswith("US"), True)
ok &= check("VHT 80/160", "80MHz" in ies.decode_vht_caps(bytes([0x03, 0, 0, 0])), True)
ok &= check("rates 1&2M", "1.0" in ies.decode_rates(bytes([0x82, 0x84])) and "2.0" in ies.decode_rates(bytes([0x82, 0x84])), True)

print("\n== decode_ie dispatch ==")
ok &= check("tag 48 dispatched", ies.decode_ie(48, rsn)[0] == "RSN/security", True)
ok &= check("unknown tag none", ies.decode_ie(99, b"\x01")[1], None)

print()
print("ALL PASS" if ok else "SOME FAILURES")
sys.exit(0 if ok else 1)