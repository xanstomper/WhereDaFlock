#!/usr/bin/env python3
"""
WhereDaFlock - tests for the passive packet analyzer.

Verifies the pure-Python 802.11 and BLE decoders in packet_analyzer.py using
synthetic frames, so the parsing logic is regression-protected without needing
capture hardware or scapy.

Run:  python3 firmware/tests/test_packet_analyzer.py
"""

import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import packet_analyzer as pa


def build_ad(atype, data):
    """Construct a BLE advertising-data struct: [len][type][data]."""
    return bytes([1 + len(data), atype]) + data


def build_probe_request(tx_mac_hex="826BF214073A", wildcard=True):
    fc = (0 << 2) | (4 << 4)  # management (type 0), probe request (subtype 4)
    p = struct.pack("<H", fc) + b"\x00\x00"
    p += bytes.fromhex("FF" * 6)                      # add1 broadcast
    p += bytes.fromhex(tx_mac_hex)                    # add2 transmitter
    p += bytes.fromhex("00" * 6)                      # add3
    p += b"\x00\x00"                                  # seq
    p += b"\x00\x00" if wildcard else b"\x00\x05H7000"  # SSID element
    p += b"\x01\x04\x82\x84\x8b\x96"                  # supported rates
    p += b"\xdd\x03\x01\x02\x03"                      # vendor-specific IE
    return p


def check(label, got, want):
    ok = got == want
    print(f"  [{'ok ' if ok else 'FAIL'}] {label}: got={got!r}")
    return ok


ok = True

print("== 802.11 probe request ==")
frame = build_probe_request("826BF214073A")
w = pa.analyze_wifi(frame)
ok &= check("identifies Probe Request", "Probe Request" in w, True)
ok &= check("decodes transmitter MAC", "82:6B:F2:14:07:3A" in w.upper(), True)
ok &= check("flags wildcard SSID", "WILDCARD SSID" in w, True)
ok &= check("walks vendor IE", "Vendor Specific" in w, True)

print("\n== BLE advertisement ==")
ad = build_ad(0x09, b"FS Ext Battery")
ad += build_ad(0xFF, bytes([0xC8, 0x09, 0x01, 0x02, 0x03]))  # FLOCK mfr
b = pa.parse_ble_ad(ad)
ok &= check("decodes device name", "'FS Ext Battery'" in b, True)
ok &= check("detects Flock Company ID", "0x09C8" in b, True)
ok &= check("calls out FLOCK", "FLOCK" in b, True)

print("\n== boundary cases ==")
ok &= check("short frame handled", pa.analyze_wifi(b"\x00" * 10) is not None, True)
ok &= check("empty BLE handled", pa.parse_ble_ad(b"") == "", True)

print("\n== beacon frame + RSSI + MAC bits ==")


def build_beacon(ssid=b"Test-Ap", tx_hex="70C94E000001"):
    fc = (0 << 2) | (8 << 4)  # management, beacon
    p = struct.pack("<H", fc) + b"\x00\x00"
    p += bytes.fromhex("FF" * 6)            # add1 broadcast DA
    p += bytes.fromhex(tx_hex)              # add2 SA (transmitter)
    p += bytes.fromhex(tx_hex)              # add3 BSSID
    p += b"\x00\x00"                        # seq control
    p += b"\x00" * 8 + b"\x64\x00" + b"\x00\x01"   # timestamp(8) + interval(2) + capability(2) = 12
    p += bytes([0, len(ssid)]) + ssid       # SSID IE
    p += b"\x01\x08\x82\x84\x8B\x96\x0C\x12\x18\x24"  # rates
    return p


beacon = build_beacon()
w = pa.analyze_wifi(beacon)
ok &= check("beacon identified", "Beacon" in w, True)
ok &= check("beacon SSID read", "'Test-Ap'" in w, True)

d = pa.wifi_to_dict(beacon, rssi=-65)
ok &= check("dict ok", d.get("ok"), True)
ok &= check("dict type", d.get("type") == "Management", True)
ok &= check("dict ssid", d.get("ssid") == "Test-Ap", True)
ok &= check("distance in dict", d.get("dist_m") is not None, True)
ok &= check("tx randomized flag", d.get("transmitter_randomized") is False, True)  # 70:C9:4E universal

print("\n== wifi_to_dict on randomized transmitter ==")
probe_rand = build_probe_request("82AB02F10000")
dr = pa.wifi_to_dict(probe_rand, rssi=-50)
ok &= check("randomized flag true", dr.get("transmitter_randomized"), True)
ok &= check("wildcard_probe flag", dr.get("wildcard_probe"), True)

print()
print("ALL PASS" if ok else "SOME FAILURES")
sys.exit(0 if ok else 1)