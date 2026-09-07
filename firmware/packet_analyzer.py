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
import json
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
    """Walk Information Elements (TLV: tag, len, value) and annotate known tags.
    Yields (tag, name, len, description, raw_value) tuples."""
    import ie_decode as ies
    p = 0
    tags = []
    while p + 2 <= len(body):
        tag = body[p]
        ln = body[p + 1]
        val = bytes(body[p + 2 : p + 2 + ln])
        if p + 2 + ln > len(body):
            break
        name = table.get(tag, f"tag#{tag}")
        # Deep decode when we have a decoder.
        _, ddesc = ies.decode_ie(tag, val)
        if tag == 0:  # SSID
            desc = val.decode("utf-8", "replace") or "(wildcard/empty)"
        elif tag == 1:  # Supported Rates
            desc = ies.decode_rates(val)
        elif tag == 3:  # DS / channel
            desc = f"channel {val[0] if val else '?'}"
        elif tag == 221:  # Vendor-specific
            desc = f"vendor element, {ln} bytes"
        elif ddesc:
            desc = ddesc
        else:
            desc = f"({ln} bytes)"
        tags.append((tag, name, ln, desc, val))
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


def analyze_wifi(payload, show_mac_bits=True):
    """Full readable breakdown of an 802.11 frame."""
    parsed = parse_80211_header(payload)
    if not parsed:
        return "  <short/fragment; cannot parse header>"
    fc, ftype, fsub, a1, a2, a3 = parsed
    tname = FRAME_TYPES.get(ftype, f"type{ftype}")
    sname = MGMT_SUBTYPES.get(fsub, f"subtype{fsub}") if ftype == 0 else f"sub{fsub}"
    protected = bool(fc & 0x4000)      # Protected Frame bit
    retry = bool(fc & 0x0800)          # Retry bit
    more_data = bool(fc & 0x2000)

    import signal_math as sm
    lines = [
        f"  Frame:  {tname} / {sname}  (type={ftype} subtype={fsub})",
        f"           +CRYPTO protected  +retry  +more-data",
        f"           flags: protect={int(protected)} retry={int(retry)} more_data={int(more_data)}",
        f"           add1(DA/BSSID): {a1}",
        f"           add2(SA/TA):    {a2}  [{sm.mac_randomization_label(a2)}]",
        f"           add3(BSSID):    {a3}",
    ]
    if ftype == 1:  # Control frames
        # Common control subtypes: 8=BlockAck, 9=BlockAckReq, 10=PS-Poll,
        # 11=RTS, 12=CTS, 13=ACK, 14=CF-End, 15=CF-End+CF-Ack
        ctl = {8: "Block Ack", 9: "Block Ack Request", 10: "PS-Poll",
               11: "RTS", 12: "CTS", 13: "ACK", 14: "CF-End", 15: "CF-End+CF-Ack"}
        cname = ctl.get(fsub, sname)
        lines.append(f"  Control: {cname}")
        if fsub in (11, 12, 13):  # RTS/CTS/ACK carry a receiver address in a1
            lines.append(f"           to {a1}")
    elif ftype == 2:  # Data frames
        qos = bool(fc & 0x0800)  # QoS subfield shares the retry bit for data? use ToDS/FromDS
        ds_from = bool(fc & 0x0100)
        ds_to = bool(fc & 0x0200)
        lines.append(f"  Data: {'QoS ' if qos else ''}ToDS={int(ds_to)} FromDS={int(ds_from)}")
    elif ftype == 0:  # Management — advance past any fixed body header.
        fixed = {1: 8, 3: 6, 5: 12, 8: 12, 11: 6}.get(fsub, 0)  # assoc/resp, probe-resp/beacon, auth
        body = payload[24 + fixed:]

        tags = parse_elements(body, IE_TABLE)
        lines.append(f"  {sname} body:")
        for tag, name, ln, desc, val in tags:
            marker = "◄ WILDCARD SSID" if (tag == 0 and desc == "(wildcard/empty)") else "  "
            if name == "SSID" and desc != "(wildcard/empty)":
                marker = "● SSID"
            lines.append(f"        {marker} IE {name:<20} len={ln:<3} {desc}")

        if fsub == 4 and tags and tags[0][0] == 0 and tags[0][3] == "(wildcard/empty)":
            lines.append("  ➜ WILDCARD probe request (any AP may respond).")
        if fsub == 8:  # Beacon
            ssid = next((t[3] for t in tags if t[0] == 0 and t[3] != "(wildcard/empty)"), "?")
            lines.append(f"  ➜ Beacon for SSID {ssid!r}")
    return "\n".join(lines)


def wifi_to_dict(payload, rssi=None):
    """Structured dict for a WiFi frame (drives JSON/pcap summaries)."""
    import signal_math as sm
    parsed = parse_80211_header(payload)
    d = {"ok": False}
    if not parsed:
        return d
    fc, ftype, fsub, a1, a2, a3 = parsed
    d = {
        "protocol": "wifi_2_4ghz",
        "type": FRAME_TYPES.get(ftype),
        "subtype": MGMT_SUBTYPES.get(fsub) if ftype == 0 else f"sub{fsub}",
        "add1": a1, "add2": a2, "add3": a3,
        "protected": bool(fc & 0x4000),
        "retry": bool(fc & 0x0800),
        "transmitter_randomized": sm.is_locally_administered(a2),
        "ok": True,
    }
    if rssi is not None:
        d["rssi"] = rssi
        d["dist_m"] = sm.estimate_distance(rssi)
        d["range_label"] = sm.rssi_to_distance_rough(rssi)
    if ftype == 0 and fsub in (0, 1, 2, 3, 4, 5, 8, 11, 13):
        fixed = {1: 8, 3: 6, 5: 12, 8: 12, 11: 6}.get(fsub, 0)
        body = payload[24 + fixed:]
        tags = parse_elements(body, IE_TABLE)
        d["elements"] = [{"id": t[0], "name": t[1], "len": t[2], "detail": t[3]} for t in tags]
        d["ssid"] = next((t[3] for t in tags if t[0] == 0 and t[3] != "(wildcard/empty)"), None)
        if fsub == 4:
            d["wildcard_probe"] = any(t[0] == 0 and t[3] == "(wildcard/empty)" for t in tags)
    return d


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


# Common 16-bit GATT service UUIDs (for AD types 0x02-0x07)
GATT_16 = {
    0x1800: "Generic Access", 0x1801: "Generic Attribute", 0x180A: "Device Info",
    0x180F: "Battery", 0x1812: "HID", 0x1819: "Location & Navigation",
    0x1802: "Immediate Alert", 0x1803: "Link Loss", 0x1805: "Current Time",
    0x180D: "Heart Rate", 0x180E: "Phone Alert", 0x1810: "Blood Pressure",
    0x1811: "Alert Notification", 0x181A: "Environmental Sensing",
    0x181D: "Body Composition", 0x181E: "Body Composition",
    0x181F: "Continuous Glucose", 0x181C: "User Data", 0x1822: "Pulse Oximeter",
}
# Common BLE appearance codes (AD type 0x19, 2-byte LE)
GATT_APPEARANCE = {0x0040: "Generic Phone", 0x00C0: "Generic Computer",
                   0x00C5: "Laptop", 0x0180: "Generic Watch", 0x01C0: "Generic Tag"}


def _uuid16_name(u16):
    return GATT_16.get(u16, f"0x{u16:04X}")


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
        elif atype in (0x02, 0x03):  # 16-bit service UUIDs (LE)
            uuids = [data[i] | (data[i + 1] << 8) for i in range(0, len(data) - 1, 2)]
            desc = ", ".join(f"{_uuid16_name(u)}(0x{u:04X})" for u in uuids)
        elif atype in (0x06, 0x07):  # 128-bit UUIDs
            desc = f"{len(data) // 16} x 128-bit UUID(s)"
        elif atype == 0x0A:
            desc = f"TX power: {struct.unpack('<b', bytes([data[0]]))[0] if data else '?'} dBm"
        elif atype == 0x19:  # Appearance (2-byte LE)
            if len(data) >= 2:
                app = data[0] | (data[1] << 8)
                desc = GATT_APPEARANCE.get(app, f"0x{app:04X}")
        elif atype == 0x01 and data:  # Flags
            desc = f"flags=0x{data[0]:02X}"
        elif atype == 0xFF and len(data) >= 2:
            co = data[0] | (data[1] << 8)
            is_flock = co == FLOCK_MFR_ID
            desc = (f"CompanyID=0x{co:04X} ({'FLOCK' if is_flock else 'vendor'}), "
                    f"payload={data[2:].hex() or '(none)'}")
            if is_flock:
                lines.append("  ➜ Flock manufacturer Company ID 0x09C8 detected!")
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
def analyze_pcap(path, max_frames=50, out_json=None, summary=True):
    import signal_math as sm
    from scapy.all import rdpcap

    print(f"Analyzing {path} (up to {max_frames} frames)...")
    frames = []
    shown = 0
    for pkt in rdpcap(path):
        try:
            from scapy.layers.dot11 import Dot11
            if not pkt.haslayer(Dot11):
                continue
            raw = bytes(pkt)
            rssi = getattr(pkt, "dBm_AntSignal", None) or getattr(pkt, "signal", None)
            d = wifi_to_dict(raw, rssi=int(rssi) if rssi is not None else None)
            if not d.get("ok"):
                continue
            print(analyze_wifi(raw))
            print()
            frames.append(d)
            shown += 1
            if shown >= max_frames:
                break
        except Exception:
            continue

    if shown == 0:
        print("No 802.11 frames found in this capture.")
        return

    if summary:
        from collections import Counter
        subtypes = Counter((f["type"], f["subtype"]) for f in frames)
        rand = sum(1 for f in frames if f.get("transmitter_randomized"))
        print("=" * 60)
        print("PCAP SUMMARY")
        print(f"  frames decoded   : {len(frames)}")
        print(f"  randomized addrs : {rand}/{len(frames)} transmitter MACs"
              " (locally-administered / random-bit set)")
        print("  frame types:")
        for (typ, sub), n in subtypes.most_common():
            print(f"    {typ:<11} / {sub:<22} n={n}")
        dists = [f["dist_m"] for f in frames if "dist_m" in f]
        if dists:
            print(f"  est. distance    : min={min(dists)}m  max={max(dists)}m  (RSSI model)")
        print("=" * 60)

    if out_json:
        with open(out_json, "w") as f:
            json.dump(frames, f, indent=2)
        print(f"Wrote structured JSON to {out_json} ({len(frames)} frames)")


def main():
    ap = argparse.ArgumentParser(description="WhereDaFlock passive packet analyzer")
    ap.add_argument("--pcap", metavar="FILE", help="analyze an offline .pcap")
    ap.add_argument("--live", metavar="IFACE", help="live capture on a monitor-mode iface")
    ap.add_argument("--count", type=int, default=20)
    ap.add_argument("--json", metavar="FILE", help="also write structured JSON")
    ap.add_argument("--no-summary", action="store_true", help="skip the pcap summary")
    args = ap.parse_args()

    try:
        import scapy  # noqa
    except ImportError:
        sys.exit("Install scapy first: pip install scapy")

    if args.pcap:
        analyze_pcap(args.pcap, args.count, out_json=args.json,
                     summary=not args.no_summary)
    elif args.live:
        live_capture(args.live, args.count)
    else:
        ap.error("provide --pcap FILE or --live IFACE")


if __name__ == "__main__":
    main()