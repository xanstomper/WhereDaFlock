#!/usr/bin/env python3
"""
WhereDaFlock - unit tests for Multi-Threat Signatures & Categories.

Verifies:
  - Flock ALPR camera detection
  - Police vehicle MDT detection (Sierra Wireless, Cradlepoint)
  - Police body camera detection (Axon OUI + BLE MFR/UUID)
  - Secondary ALPR detection (Vigilant Solutions, Genetec)
  - Commercial CCTV surveillance detection (Axis, Dahua, Hikvision)
  - Drone Remote ID detection (OpenDroneID OUI + BLE 0xFFFA)
  - Sub-GHz Police Radio command and frequency parsing
"""

import os
import sys

def check(label, got, want):
    status = "ok " if got == want else "FAIL"
    print(f"  [{status}] {label}: got={got!r} want={want!r}")
    return got == want

ok = True

# Multi-Threat OUI mappings (mirrors signatures.h)
ALL_TARGET_OUIS = {
    # Flock
    "70C94E": ("CAT_FLOCK_ALPR", "Flock Safety"),
    "826BF2": ("CAT_FLOCK_ALPR", "Flock Safety"),
    # Police Vehicles (MDTs)
    "000F70": ("CAT_POLICE_VEHICLE", "Sierra Wireless"),
    "003044": ("CAT_POLICE_VEHICLE", "Cradlepoint Inc"),
    # Police Body Cameras
    "8887C0": ("CAT_POLICE_BODYCAM", "Axon Enterprise"),
    "0025DF": ("CAT_POLICE_BODYCAM", "Axon Enterprise"),
    # Other ALPRs
    "0023CD": ("CAT_OTHER_ALPR", "Vigilant / Moto"),
    "001AE8": ("CAT_OTHER_ALPR", "Genetec Inc"),
    # CCTV Surveillance
    "00408C": ("CAT_SURVEILLANCE_CAM", "Axis Communications"),
    "3CEF8C": ("CAT_SURVEILLANCE_CAM", "Dahua Technology"),
    "40B4CD": ("CAT_SURVEILLANCE_CAM", "Hikvision"),
    # Drone Remote ID
    "FA0BBC": ("CAT_DRONE_UAV", "OpenDroneID"),
}

def match_oui(mac):
    clean = "".join(mac.split(":")).upper().replace("-", "")[:6]
    return ALL_TARGET_OUIS.get(clean, None)

print("== Multi-Threat OUI Matching ==")
ok &= check("Flock OUI match", match_oui("70:c9:4e:11:22:33")[0], "CAT_FLOCK_ALPR")
ok &= check("Police MDT Sierra match", match_oui("00:0f:70:44:55:66")[0], "CAT_POLICE_VEHICLE")
ok &= check("Police MDT Cradlepoint match", match_oui("00:30:44:aa:bb:cc")[0], "CAT_POLICE_VEHICLE")
ok &= check("Axon Bodycam match", match_oui("88:87:c0:99:88:77")[0], "CAT_POLICE_BODYCAM")
ok &= check("Vigilant ALPR match", match_oui("00:23:cd:12:34:56")[0], "CAT_OTHER_ALPR")
ok &= check("Hikvision CCTV match", match_oui("40:b4:cd:fe:dc:ba")[0], "CAT_SURVEILLANCE_CAM")
ok &= check("Drone Remote ID match", match_oui("fa:0b:bc:00:11:22")[0], "CAT_DRONE_UAV")
ok &= check("Consumer device no match", match_oui("98:d6:bb:11:22:33"), None)

print("\n== BLE Multi-Threat Matching ==")
# Test Axon MFR ID (0x0283) and Flock (0x09C8)
AXON_MFR_ID = 0x0283
FLOCK_MFR_ID = 0x09C8
DRONE_RID_UUID = "FFFA"
AXON_SIGNAL_UUIDS = ["FE01", "FE02"]

def match_ble(mfr_id=None, service_uuids=None):
    if mfr_id == FLOCK_MFR_ID:
        return "CAT_FLOCK_ALPR"
    if mfr_id == AXON_MFR_ID:
        return "CAT_POLICE_BODYCAM"
    if service_uuids:
        for u in service_uuids:
            if any(sig in u.upper() for sig in AXON_SIGNAL_UUIDS):
                return "CAT_POLICE_BODYCAM"
            if DRONE_RID_UUID in u.upper():
                return "CAT_DRONE_UAV"
    return "UNKNOWN"

ok &= check("BLE Flock battery mfr", match_ble(mfr_id=0x09C8), "CAT_FLOCK_ALPR")
ok &= check("BLE Axon Enterprise mfr", match_ble(mfr_id=0x0283), "CAT_POLICE_BODYCAM")
ok &= check("BLE Axon Signal sync UUID", match_ble(service_uuids=["0000fe01-0000-1000-8000-00805f9b34fb"]), "CAT_POLICE_BODYCAM")
ok &= check("BLE Drone Remote ID UUID", match_ble(service_uuids=["0000fffa-0000-1000-8000-00805f9b34fb"]), "CAT_DRONE_UAV")

print("\n== Police Radio Serial Bridge Parsing ==")
import re

def parse_radio_cmd(cmd_line):
    m = re.match(r"^RADIO\s+([\d.]+)\s+(\S+)\s+([-\d]+)\s+(.*)$", cmd_line)
    if not m:
        return None
    return {
        "freq": float(m.group(1)),
        "proto": m.group(2),
        "rssi": int(m.group(3)),
        "desc": m.group(4).strip()
    }

cmd = "RADIO 851.2500 P25_Phase_2 -62 Metro Police Dispatch"
p = parse_radio_cmd(cmd)
ok &= check("Radio parsed freq", p["freq"], 851.25)
ok &= check("Radio parsed proto", p["proto"], "P25_Phase_2")
ok &= check("Radio parsed rssi", p["rssi"], -62)
ok &= check("Radio parsed desc", p["desc"], "Metro Police Dispatch")

print()
print("ALL PASS" if ok else "SOME FAILURES")
sys.exit(0 if ok else 1)
