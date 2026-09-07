#!/usr/bin/env python3
"""
WhereDaFlock - host-side passive 2.4GHz WiFi detector (Beta).

A cross-platform companion to the ESP32 firmware. It implements the same
Flock Cam detection logic so you can prototype and verify matching on a
computer with a monitor-mode WiFi adapter before/alongside the microcontroller
build. It mirrors the firmware's OUI + wildcard-probe/IE confidence tiers.

PASSIVE ONLY: this script never transmits probes and never associates to a
network. It only listens.

Requires a monitor-mode capable adapter and a packet cature library, e.g.:
    Linux:  sudo pip install scapy
    (adapter must support monitor mode: `iw dev wlan0 set type monitor`)

Usage:
    python3 host_scanner.py --scan 20
    python3 host_scanner.py --scan 30 --json results.json
"""

import argparse
import json
import sys
import time
from datetime import datetime, timezone

# ---------------------------------------------------------------------------
# Signatures (mirrors firmware/src/signatures.h)
# ---------------------------------------------------------------------------
TARGET_OUIS = [
    "70:c9:4e", "3c:91:80", "d8:f3:bc", "80:30:49", "b8:35:32",
    "14:5a:fc", "74:4c:a1", "08:3a:88", "9c:2f:9d", "c0:35:32",
    "94:08:53", "e4:aa:ea", "f4:6a:dd", "e0:0a:f6", "24:b2:b9",
    "00:f4:8d", "d0:39:57", "e8:d0:fc", "e0:4f:43", "b8:1e:a4",
    "70:08:94", "58:8e:81", "ec:1b:bd", "3c:71:bf", "58:00:e3",
    "90:35:ea", "5c:93:a2", "64:6e:69", "48:27:ea", "a4:cf:12",
    "14:b5:cd", "82:6b:f2",
]

TIER_SSID, TIER_ECHO, TIER_OUI, TIER_PROBE, TIER_IE_SIG = 0, 1, 2, 3, 4

# Normalize OUIs to a set of 6-hex-char uppercase strings for fast lookup.
_NORM_OUIS = {"".join(o.split(":")).upper() for o in TARGET_OUIS}


def _oui_of(mac: str) -> str:
    """Return the OUI (first 3 bytes, no colons) of a MAC, uppercased."""
    m = "".join(mac.split(":")).upper().replace("-", "")
    return m[:6]


def is_target_oui(mac: str) -> bool:
    return _oui_of(mac) in _NORM_OUIS


def tier_of(mac: str, frame_type: str, subtype: int | None, eds: dict) -> tuple[int, str, bool]:
    """
    Return (tier, method, wildcard_probe) for a given frame + its parsed IE
    element dictionary. Mirrors the firmware tier assignment:
      4  wildcard_probe_ie_sig   OUI + wildcard SSID probe + vendor IE present
      3  wildcard_probe          OUI + wildcard SSID probe, no vendor IE
      2  oui_addr2               transmitter OUI on any frame
      1  oui_addr1/addr3         receiver/BSSID OUI
    """
    wildcard = False
    vendor_ie = False

    if frame_type == 0 and subtype == 4:   # Management, Probe Request
        # Wildcard probe: SSID element with length 0.
        if eds.get("ssid", b"") == b"":
            wildcard = True
        vendor_ie = bool(eds.get("vendor"))

    if wildcard:
        return (TIER_IE_SIG, "wildcard_probe_ie_sig", True) if vendor_ie \
               else (TIER_PROBE, "wildcard_probe", True)
    return (TIER_OUI, "oui_addr2", False)


def classify(mac: str, frame_type: int, subtype: int, eds: dict, addr1: str = None,
             addr3: str = None, addr_from: str = None) -> tuple[int, str]:
    """
    Highest-confidence tier for a frame given its transmitter (addr_from) MAC.
    Falls back to addr1/addr3 echo tier only if the transmitter OUI didn't hit.
    """
    if is_target_oui(addr_from or mac):
        tier, method, _ = tier_of(mac, frame_type, subtype, eds)
        return tier, method
    # Transmitter not a known OUI; check receiver / BSSID echo paths (tier 1).
    if (addr1 and is_target_oui(addr1)) or (addr3 and is_target_oui(addr3)):
        return TIER_ECHO, "oui_addr1_addr3"
    return -1, ""


# ---------------------------------------------------------------------------
# Live capture (Scapy, monitor mode)
# ---------------------------------------------------------------------------
_found = {}


def _handle_pkt(pkt):
    try:
        from scapy.layers.dot11 import Dot11, Dot11ProbeReq
        if not pkt.haslayer(Dot11):
            return
        d = pkt[Dot11]
        addr2 = d.addr2
        if not addr2 or not is_target_oui(addr2):
            # Fall back to addr1/addr3 echo paths
            a1, a3 = d.addr1, d.addr3
            if not ((a1 and is_target_oui(a1)) or (a3 and is_target_oui(a3))):
                return
        rssi = getattr(pkt, "dBm_AntSignal", None)
        if rssi is None:
            rssi = getattr(pkt, "signal", None) or getattr(pkt, "RSSI", None)
        if rssi is None:
            rssi = 0

        # Determine frame type/subtype from Dot11 header FC field.
        fc = d.fc_field if hasattr(d, "fc_field") else 0
        ftype = (fc >> 2) & 0x3
        fsubtype = (fc >> 4) & 0x0F

        # Build IE dict for probe requests.
        eds = {}
        if pkt.haslayer(Dot11ProbeReq):
            pr = pkt[Dot11ProbeReq]
            ssid = pr.info if hasattr(pr, "info") else b""
            # Heuristic vendor-IE probe: a probe with a nonempty vendor tail.
            sector = pkt
            vendor = getattr(pr, "vendor", b"")
            eds = {"ssid": ssid, "vendor": vendor}

        tier, method = classify(addr2 or "", ftype, fsubtype, eds,
                                d.addr1, d.addr3, addr2)
        if tier < 0:
            return

        key = addr2 or d.addr1 or "??"
        entry = {
            "ts": datetime.now(timezone.utc).isoformat(),
            "mac": key,
            "rssi": rssi,
            "tier": tier,
            "method": method,
            "channel": getattr(pkt, "channel", None) or "",
        }
        _found[key] = entry
        print(f"[tier {tier}] {key}  RSSI {rssi:>4} dBm  method={method}")
    except Exception:
        pass


def run_scan(duration: int, iface: str):
    from scapy.all import sniff
    print(f"WhereDaFlock host scanner - passive, {duration}s on {iface} ...\n")
    sniff(iface=iface, prn=_handle_pkt, store=False, timeout=duration)


def main():
    ap = argparse.ArgumentParser(description="WhereDaFlock passive 2.4GHz WiFi detector")
    ap.add_argument("--scan", type=int, default=20, help="scan duration seconds")
    ap.add_argument("--iface", default="wlan0", help="monitor-mode interface")
    ap.add_argument("--json", default=None, help="optional JSON export path")
    args = ap.parse_args()

    try:
        run_scan(args.scan, args.iface)
    except ModuleNotFoundError:
        sys.exit("Install packet library first: sudo pip install scapy")
    except PermissionError:
        sys.exit("Root required for monitor-mode capture. Run with sudo.")

    print(f"\nDone. {len(_found)} matching transmitter(s) observed.")
    if args.json:
        with open(args.json, "w") as f:
            json.dump(list(_found.values()), f, indent=2)
        print(f"Wrote {args.json}")


if __name__ == "__main__":
    main()