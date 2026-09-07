#!/usr/bin/env python3
"""
WhereDaFlock dashboard - demo/smoke tester.

Posts a few sample WiFi + BLE detections to a running dashboard so you can
verify the UI end-to-end without an ESP32 attached.

Usage:
    python3 seed.py [--base http://localhost:5000] [--count 10]
"""

import argparse
import json
import random
import time
import urllib.request


def post(base, payload):
    req = urllib.request.Request(
        base + "/api/detections",
        data=json.dumps(payload).encode(),
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=5) as r:
        return r.status


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", default="http://localhost:5000")
    ap.add_argument("--count", type=int, default=10)
    args = ap.parse_args()

    sample_wifi_macs = [
        "82:6B:F2:14:07:3A", "70:C9:4E:01:02:03", "3C:91:80:AA:BB:CC",
        "B8:35:32:11:22:33", "E0:0A:F6:99:00:11",
    ]
    sample_ble_macs = ["D4:E9:F4:B1:40:0C", "A8:3B:76:12:34:56"]

    print(f"Seeding {args.count} detections to {args.base} ...")
    for i in range(args.count):
        if i % 3 == 0:
            payload = {
                "protocol": "wifi_2_4ghz",
                "mac_address": random.choice(sample_wifi_macs),
                "rssi": random.randint(-85, -40),
                "detection_method": "wifi_wildcard_probe_ie_sig",
                "detection_tier": 4,
                "verdict": "FLOCK_LIKELY",
                "channel": random.choice([1, 6, 11]),
                "frequency": 2437,
            }
        elif i % 3 == 1:
            payload = {
                "protocol": "ble",
                "mac": random.choice(sample_ble_macs),
                "name": "FS Ext Battery",
                "rssi": random.randint(-75, -45),
                "method": "mfr_id",
                "mfr_id": "0x09C8",
                "verdict": "FLOCK_LIKELY",
            }
        else:
            payload = {
                "protocol": "wifi_2_4ghz",
                "mac_address": "D4:E9:F4:B1:40:0C",
                "rssi": random.randint(-80, -50),
                "detection_method": "wifi_oui_addr2",
                "detection_tier": 2,
                "channel": 6,
            }
        status = post(args.base, payload)
        print(f"  [{i+1}] status {status} {payload['protocol']} rssi {payload.get('rssi') or payload.get('rssi')}")
        time.sleep(0.3)
    print("Done. Open the dashboard.")


if __name__ == "__main__":
    main()