#!/usr/bin/env python3
"""
WhereDaFlock - tests for the batch aggregator (no capture hardware needed).
Run:  python3 firmware/tests/test_analyze_batch.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import analyze_batch  # noqa


def check(label, got, want):
    ok = got == want
    print(f"  [{'ok ' if ok else 'FAIL'}] {label}")
    return ok


ok = True

# Build synthetic wifi_to_dict records.
import packet_analyzer as pa
import struct


def mk_probe(tx, rssi):
    fc = (0 << 2) | (4 << 4)
    p = struct.pack("<H", fc) + b"\x00\x00"
    p += bytes.fromhex("FF" * 6)
    p += bytes.fromhex(tx)
    p += bytes.fromhex("000000000000")
    p += b"\x00\x00"
    p += b"\x00\x00"              # wildcard SSID
    return pa.wifi_to_dict(p, rssi=rssi)


print("== aggregate math ==")
records = {
    "a.pcap": [mk_probe("826BF214073A", -55), mk_probe("826BF214073A", -60),
               mk_probe("70C94E000001", -70), mk_probe("82AB02F10000", -65)],
    "b.pcap": [mk_probe("826BF214073A", -58)],
}

# Monkeypatch load_frames to return our synthetic records.
def fake_load(path, limit=200000):
    return records.get(path, [])
analyze_batch.load_frames = fake_load

per_file, agg = analyze_batch.aggregate(["a.pcap", "b.pcap"])
ok &= check("2 files", len(agg["files"]), 2)
ok &= check("5 total frames", agg["frames"], 5)
ok &= check("3 unique transmitters", len(agg["transmitters"]), 3)
# 0x82 -> randomized bit set; four frames use 0x82-prefixed addrs (3x 826BF2 + 1x 82AB)
ok &= check("4 randomized frames", agg["randomized"], 4)
# Flock-OUI frames: 826BF2 x3 + 70C94E x1 = 4 (82:AB is random, not a Flock OUI)
ok &= check("826BF2 + 70C94E flock (4 frames)", sum(agg["flock_like"].values()), 4)
# top transmitter by frame count
top = agg["transmitters"].most_common(1)[0]
ok &= check("top transmitter 826BF2", top[0], "82:6B:F2:14:07:3A")
ok &= check("top count 3", top[1], 3)

rep = analyze_batch.format_report(agg, per_file)
ok &= check("report mentions Flock", "Flock-OUI" in rep, True)
ok &= check("report lists per-file", "a.pcap" in rep and "b.pcap" in rep, True)

print()
print("ALL PASS" if ok else "SOME FAILURES")
sys.exit(0 if ok else 1)