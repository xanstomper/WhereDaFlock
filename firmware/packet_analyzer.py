#!/usr/bin/env python3
"""
WhereDaFlock - passive packet analyzer (educational).

Decodes and explains the 802.11 and BLE frames that Flock-style surveillance
hardware broadcasts, frame-by-frame. This is PASSIVE analysis: it reads
captured packets and prints a readable breakdown of their structure.

Two input modes:
  1. Live capture from a monitor-mode interface (needs scapy + root).
  2. An offline .pcap file (no capture hardware needed once you have a file).

This complements the detector: where WhereDaFlock_scanner.ino just *matches*
signatures, this tool shows you *why* a frame matches by walking its bytes.

Examples:
  python3 packet_analyzer.py --pcap capture.pcap            # offline analysis
  sudo python3 packet_analyzer.py --live wlan0 --count 5    # live (root)

REQUIRES: pip install scapy
"""

import argparse
import struct
import sys


# ---------------------------------------------------------------------------
# 802.11 helpers
# ---------------------------------------------------------------------------
FRAME_TYPES = {0: "Management", 1: "Control", 2: "Data", 3: "Extension"}
MGMT_SUBTYPES = {
    0: "Association Request", 1: "Association Response",
    2: "Reassociation Request", 3: "Reassociation Response",
    4: "Probe Request", 5: "Probe Response", 8: "Beacon",
    9: "ATIM", 10: "Disassociation", 11: "Authentication",
    12: "Deauthentication", 13: "Action",
}


def parse_80211_header(payload):
    """Return (frame_type, subtype, addr1, addr2, addr3) from a Dot11 frame."""
    if len(payload) < 24:
        return None
    fc = struct.unpack("<H", payload[0:2])[0]
    ftype = (fc >> 2) & 0x3
    fsub = (fc >> 4) & 0xF
    a1 = ":".join(f"{b:02X}" for b in payload[4:10])
    a2 = ":".join(f"{b:02X}" for b in payload[10:16])
    a3 = ":".join(f"{b:02X}" for b in payload[16:22])
    return (fc, ftype, fsub, a1, a2, a3)


def parse_elements(body, table):
    """Walk Information Elements (TLV: tag, len, value) and annotate known tags."""
    p = 0
    tags = []
    while p + 2 <= len(body):
        tag = body[p]
        ln = body[p + 1]
        val = body[p + 2 : p + 2 + ln]
        if p + 2 + ln > len(body):
            break
        name = table.get(tag, f"tag#{tag}")
        desc = ""
        if tag == 0:  # SSID
            desc = val.decode("utf-8", "replace") or "(wildcard/empty)"
        elif tag == 1:  # Supported Rates
            desc = " ".join(str(b >> 1) for b in val)
        elif tag == 3:  # DS / channel
            desc = f"channel {val[0] if val else '?'}"
        elif tag == 221:  # Vendor-specific
            desc = f"vendor element, {ln} bytes"
        tags.append((tag, name, ln, desc))
        p += 2 + ln
    return tags


IE_TABLE = {
    0: "SSID", 1: "Supported Rates", 3: "DS Parameter Set", 4: "CF Parameter Set",
    5: "TIM", 6: "IBSS Parameter Set", 7: "Country", 8: "Hopping Params",
    10: "Request", 11: "BSS Load", 12: "EDCA", 13: "TSPEC", 32: "Power Constraint",
    33: "Power Capability", 35: "TPC Report", 42: "ERP", 45: "HT Capabilities",
    48: "RSN", 50: "Extended Supported Rates", 61: "HT Information",
    127: "Extended Capabilities", 221: "Vendor Specific", 191: "VHT Capabilities",
}


def analyze_wifi(payload):
    """Full readable breakdown of an 802.11 frame."""
    parsed = parse_80211_header(payload)
    if not parsed:
        return "  <short/fragment; cannot parse header>"
    fc, ftype, fsub, a1, a2, a3 = parsed
    tname = FRAME_TYPES.get(ftype, f"type{ftype}")
    sname = MGMT_SUBTYPES.get(fsub, f"subtype{fsub}") if ftype == 0 else f"sub{fsub}"

    lines = [f"  Frame:  {tname} / {sname}  (type={ftype} subtype={fsub})",
             f"           add1(DA/BSSID): {a1}",
             f"           add2(SA/TA)   : {a2}",
             f"           add3(BSSID)  : {a3}"]
    if ftype == 0 and fsub == 4:  # Probe Request
        body = payload[24:]
        tags = parse_elements(body, IE_TABLE)
        wild = tags and tags[0][0] == 0 and tags[0][3] == "(wildcard/empty)"
        lines.append(f"  Probe Request body:")
        for tag, name, ln, desc in tags:
            marker = "◄ WILDCARD SSID" if (tag == 0 and desc == "(wildcard/empty)") else "  "
            lines.append(f"        {marker} IE {name:<24} len={ln:<3} {desc}")
        if wild and tags:
            lines.append("  ➜ This is a WILDCARD probe request (any AP may respond).")
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# BLE advertising helpers
# ---------------------------------------------------------------------------
BLE_AD_TYPES = {
    0x01: "Flags", 0x02: "Incomplete 16-bit UUIDs", 0x03: "Complete 16-bit UUIDs",
    0x04: "Incomplete 32-bit UUIDs", 0x05: "Complete 32-bit UUIDs",
    0x06: "Incomplete 128-bit UUIDs", 0x07: "Complete 128-bit UUIDs",
    0x08: "Short local name", 0x09: "Complete local name",
    0x0A: "TX power level", 0x16: "Service Data 16-bit UUID",
    0x19: "Appearance", 0xFF: "Manufacturer Specific",
}


FLOCK_MFR_ID = 0x09C8


def parse_ble_ad(payload):
    """Break down a BLE advertising PDU (the payload of an HCI_LE_Advertise)."""
    lines = []
    p = 0
    while p + 2 <= len(payload):
        ln = payload[p]          # length of Type byte + Data bytes
        atype = payload[p + 1]
        data_len = max(0, ln - 1)          # Data excludes the Type byte
        data = payload[p + 2 : p + 2 + data_len]
        name = BLE_AD_TYPES.get(atype, f"type#{atype}")
        desc = f"({data_len} bytes)"
        if atype in (0x08, 0x09):
            desc = f"{data.decode('utf-8', 'replace')!r}"
        elif atype == 0xFF and len(data) >= 2:
            # Manufacturer Specific: first 2 bytes are Company ID, little-endian
            co = data[0] | (data[1] << 8)
            is_flock = co == FLOCK_MFR_ID
            desc = (f"CompanyID=0x{co:04X} ({'FLOCK' if is_flock else 'vendor'}), "
                    f"payload={data[2:].hex() or '(none)'}")
            if is_flock:
                lines.append("  ➜ Flock manufacturer Company ID 0x09C8 detected!")
        elif atype == 0x0A:
            desc = f"TX power: {struct.unpack('<b', bytes([data[0]]))[0] if data else '?'} dBm"
        lines.append(f"  AD type {atype:02X} {name:<26} {desc}")
        p += 1 + ln   # AD header is [len][type][data]; len counts type+data
    return "\n".join(lines)


# ---------------------------------------------------------------------------
# Live capture (scapy) — sniff only the relevant frames
# ---------------------------------------------------------------------------
def _sniff_pkt(pkt, count_ref):
    try:
        from scapy.layers.dot11 import Dot11, Dot11ProbeReq
        from scapy.layers.bluetooth import HCI_Hdr, L2CAP_CmdHdr, HCI_LE_MetaEvent
        if pkt.haslayer(Dot11):
            raw = bytes(pkt)
            addr2 = raw[10:16]
            print(analyze_wifi(raw))
            # flag if transmitter OUI is a known Flock OUI (host_scanner set)
            try:
                from host_scanner import is_target_oui
                mac = ":".join(f"{b:02x}" for b in addr2)
                if len(mac) == 17 and is_target_oui(mac):
                    print(f"     ➜ transmitter {mac} matches a known Flock OUI")
            except Exception:
                pass
            count_ref[0] += 1
        elif pkt.haslayer(HCI_Hdr):
            # crude BLE advertise PDU detection; true 802.11+Bleak is easier
            pass
    except Exception:
        pass


def live_capture(iface, count):
    from scapy.all import sniff
    print(f"Listening passively on {iface} for {count} WiFi frames... (Ctrl+C to stop)\n")
    ref = [0]
    sniff(iface=iface, prn=lambda p: _sniff_pkt(p, ref), store=False,
          stop_filter=lambda p: ref[0] >= count)


# ---------------------------------------------------------------------------
# Offline pcap analysis (works without hardware / root)
# ---------------------------------------------------------------------------
def analyze_pcap(path, max_frames=50):
    from scapy.all import rdpcap
    print(f"Analyzing {path} (up to {max_frames} frames)...\n")
    shown = 0
    for i, pkt in enumerate(rdpcap(path)):
        try:
            from scapy.layers.dot11 import Dot11
            if pkt.haslayer(Dot11):
                print(analyze_wifi(bytes(pkt)))
                print()
                shown += 1
                if shown >= max_frames:
                    break
        except Exception:
            continue
    if shown == 0:
        print("No 802.11 frames found in this capture.")


def main():
    ap = argparse.ArgumentParser(description="WhereDaFlock passive packet analyzer")
    ap.add_argument("--pcap", metavar="FILE", help="analyze an offline .pcap")
    ap.add_argument("--live", metavar="IFACE", help="live capture on a monitor-mode iface")
    ap.add_argument("--count", type=int, default=5)
    args = ap.parse_args()

    try:
        import scapy  # noqa
    except ImportError:
        sys.exit("Install scapy first: pip install scapy")

    if args.pcap:
        analyze_pcap(args.pcap, args.count)
    elif args.live:
        live_capture(args.live, args.count)
    else:
        ap.error("provide --pcap FILE or --live IFACE")


if __name__ == "__main__":
    main()