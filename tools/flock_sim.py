#!/usr/bin/env python3
"""
WhereDaFlock - Flock Cam signal / data simulator
================================================
Replicates the 2.4GHz 802.11 emissions Flock Safety cameras produce so you
can validate the WhereDaFlock detector WITHOUT a real camera nearby.

Real Flock cameras (per firmware/src/signatures.h research notes):
  * transmit wildcard 802.11 probe requests (~0.125s interval)
  * hop channels ASCENDING (1 -> 2 -> ... -> 11)
  * their transmitter MAC is from a known Flock OUI (32 OUIs listed below)
  * top-confidence frames carry a vendor-specific IE (tag 0xFF)

Detection tiers the detector assigns (mirrors firmware):
  tier 2 : any frame whose transmitter (addr2) MAC has a Flock OUI
  tier 3 : probe request + wildcard SSID (tag 0, len 0) + Flock OUI
  tier 4 : tier 3 + a vendor-specific IE (tag 0xFF) in the body

Modes
-----
  radio    transmit real 802.11 probe requests over a monitor-mode,
           injection-capable WiFi adapter   (REQUIRES SUCH A RADIO)
  pcap     write synthetic Flock frames to a .pcap for Wireshark /
           offline inspection (no radio needed)
  replay   stream detection NDJSON lines (the exact format the ESP32 emits)
           to test the backend/dashboard pipeline (no radio needed)
  check    verify an interface is up and in monitor mode

Radio hardware note
-------------------
The built-in RTL8821CE (rtw_8821ce driver) CANNOT inject. You need a USB
WiFi adapter with TX-injection support, e.g.:
  * Atheros AR9271          (very cheap, classic, rock solid)
  * Ralink RT5370 / RT3070  (cheap, works with rt2800usb)
  * Realtek RTL8812AU       (fast, needs the aircrack-ng patched driver)
  * Realtek RTL8187         (old but fully injectable)

Get the adapter into monitor mode first:
    sudo ip link set wlan1 down
    sudo iw dev wlan1 set type monitor
    sudo ip link set wlan1 up
    sudo iw dev wlan1 set channel 1

Then run:      sudo python3 tools/flock_sim.py radio --iface wlan1
For a quick hit: sudo python3 tools/flock_sim.py radio --iface wlan1 --tier 4 --cams 2

Ethical notes: only transmit in a space you control, on equipment you own,
for a few seconds at a time. Do not use this to impersonate devices or
interfere with networks you do not own.
"""

import argparse
import json
import os
import random
import socket
import struct
import subprocess
import sys
import time

# ---------------------------------------------------------------------------
# Signatures (identical to firmware/src/signatures.h)
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

CHANNELS = list(range(1, 12))          # 2.4GHz channels 1..11
CAMERA_DWELL_MS = 125                  # real cameras re-probe ~every 125ms
CHANNEL_HOP_MS = 125                   # ascending hops, same cadence

# ---------------------------------------------------------------------------
# Frame building
# ---------------------------------------------------------------------------
def oui_bytes(oui: str) -> bytes:
    return bytes(int(x, 16) for x in oui.split(":"))


def random_mac(oui: str) -> bytes:
    return oui_bytes(oui) + bytes(random.randrange(256) for _ in range(3))


def build_radiotap() -> bytes:
    """8-byte header + FLAGS(0) + DBM_TX_POWER(16) + TX_FLAGS(0)."""
    present = 0x2 | 0x400 | 0x8000          # FLAGS, DBM_TX_POWER, TX_FLAGS
    body = bytes([0x00, 0x10, 0x00, 0x00])  # flags=0, dbm_power=16, tx_flags=0
    hdr = struct.pack("<BBHI", 0, 0, 8 + len(body), present)
    return hdr + body


def build_probe(ta: bytes, seq: int, tier: int, channel: int) -> bytes:
    """802.11 probe request: wildcard SSID (+rates), optional vendor IE."""
    fc = 0x0040                       # type=mgmt(0), subtype=probe-req(4)
    hdr = struct.pack("<HH6s6s6sH", fc, 0, b"\xff" * 6, ta,
                      b"\xff" * 6, seq & 0x0FFF)
    body = bytes([0x00, 0x00])        # SSID tag 0, len 0 = wildcard
    body += bytes([0x01, 0x08, 0x82, 0x84, 0x8B, 0x96, 0x0C, 0x12, 0x18, 0x24])
    body += bytes([0x03, 0x01, channel])                       # DS channel
    if tier >= 4:
        body += bytes([0xFF, 0x06, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05])
    return hdr + body


# ---------------------------------------------------------------------------
# Radio mode
# ---------------------------------------------------------------------------
def set_channel(iface: str, ch: int) -> bool:
    for cmd in (["iw", "dev", iface, "set", "channel", str(ch)],
                ["iwconfig", iface, "channel", str(ch)]):
        try:
            r = subprocess.run(cmd, stdout=subprocess.DEVNULL,
                               stderr=subprocess.DEVNULL)
            if r.returncode == 0:
                return True
        except FileNotFoundError:
            continue
    return False


def radio_mode(args):
    if os.geteuid() != 0:
        sys.exit("[-] radio mode needs root (raw AF_PACKET sockets): "
                 "run with sudo")
    try:
        s = socket.socket(socket.AF_PACKET, socket.SOCK_RAW,
                          socket.htons(0x0003))
        s.bind((args.iface, 0))
    except PermissionError:
        sys.exit(f"[-] no permission to open {args.iface} raw. "
                 "Use sudo.")
    except OSError as e:
        sys.exit(f"[-] cannot open {args.iface}: {e}")

    cam_macs = [random_mac(random.choice(TARGET_OUIS))
                for _ in range(args.cams)]
    rtap = build_radiotap()
    seq = random.randrange(0x1000)
    dwell = args.dwell_ms / 1000.0
    t_start = time.monotonic()
    sent = 0

    print(f"[*] Injecting on {args.iface} | {args.cams} cam(s) | "
          f"tier {args.tier} | hop {args.hop_ms}ms | dwell {args.dwell_ms}ms")
    for mac in cam_macs:
        print(f"    cam {cam_macs.index(mac)+1}: "
              f"{':'.join(f'{b:02x}' for b in mac)}")
    ch_idx = 0
    if not set_channel(args.iface, CHANNELS[0]):
        print("[!] could not set channel (iw/iwconfig missing?); "
              "frames still go out on the card's current channel")

    try:
        while True:
            if args.hop and time.monotonic() - t_start >= args.hop_ms / 1000.0:
                ch_idx = (ch_idx + 1) % len(CHANNELS)
                set_channel(args.iface, CHANNELS[ch_idx])
                t_start = time.monotonic()
            for mac in cam_macs:
                frame = rtap + build_probe(mac, seq, args.tier,
                                           CHANNELS[ch_idx])
                s.send(frame)
                seq = (seq + 9) & 0x0FFF
                sent += 1
                if args.count and sent >= args.count:
                    print(f"[*] sent {sent} frames, done")
                    return
            time.sleep(dwell)
    except KeyboardInterrupt:
        print(f"\n[*] sent {sent} frames total")


# ---------------------------------------------------------------------------
# PCAP mode (offline validation, no radio)
# ---------------------------------------------------------------------------
def pcap_mode(args):
    with open(args.out, "wb") as f:
        # pcap global header: us precision, linktype 127 (radiotap)
        f.write(struct.pack("<IHHiIII", 0xA1B2C3D4, 2, 4, 0, 0, 65535, 127))
        rtap = build_radiotap()
        cam = oui_bytes(args.oui) + bytes([0x11, 0x22, 0x33])
        seq = 0
        for i in range(args.count):
            frame = rtap + build_probe(cam, seq, args.tier,
                                       CHANNELS[i % len(CHANNELS)])
            ts = time.time()
            f.write(struct.pack("<IIII", int(ts), int((ts % 1) * 1_000_000),
                                len(frame), len(frame)))
            f.write(frame)
            seq = (seq + 9) & 0x0FFF
        print(f"[✓] wrote {args.count} frames to {args.out} "
              f"(ta={':'.join(f'{b:02x}' for b in cam)}, tier {args.tier})")


# ---------------------------------------------------------------------------
# NDJSON replay mode (backend/dashboard testing, no radio)
# ---------------------------------------------------------------------------
EVENT_BOOT = '{{"event":"boot","fw":"WhereDaFlock","version":"2.1.0",' \
             '"mode":"promiscuous","channels":"11,6,1","ouis":{}}}'
METHODS = {0: "wifi_ssid", 1: "wifi_oui_addr1_addr3", 2: "wifi_oui_addr2",
           3: "wifi_wildcard_probe", 4: "wifi_wildcard_probe_ie_sig"}


def replay_mode(args):
    macs = [random_mac(random.choice(TARGET_OUIS)) for _ in range(args.macs)]
    n = 0
    print(EVENT_BOOT.format(len(TARGET_OUIS)), flush=True)
    while not args.count or n < args.count:
        mac = macs[n % len(macs)]
        m = ':'.join(f'{b:02x}' for b in mac)
        ch = random.choice([1, 6, 11])
        d = {
            "event": "detection",
            "detection_method": METHODS[args.tier],
            "detection_tier": args.tier,
            "protocol": "wifi_2_4ghz",
            "mac_address": m,
            "oui": ':'.join(f'{b:02x}' for b in mac[:3]),
            "device_name": "",
            "rssi": random.randint(-70, -45),
            "channel": ch,
            "frequency": 2407 + 5 * ch,
            "ssid": "",
        }
        print(json.dumps(d), flush=True)
        n += 1
        time.sleep(1.0 / args.rate)


# ---------------------------------------------------------------------------
# Check mode
# ---------------------------------------------------------------------------
def check_mode(args):
    if not os.path.exists(f"/sys/class/net/{args.iface}"):
        sys.exit(f"[-] interface {args.iface} does not exist")
    try:
        r = subprocess.run(["iw", "dev", args.iface, "info"],
                           capture_output=True, text=True)
        print(r.stdout)
        if "type monitor" not in r.stdout:
            print("[!] NOT in monitor mode yet:")
            print(f"    sudo ip link set {args.iface} down")
            print(f"    sudo iw dev {args.iface} set type monitor")
            print(f"    sudo ip link set {args.iface} up")
        else:
            print("[✓] monitor mode active")
    except FileNotFoundError:
        print(f"[!] 'iw' not installed. Try: sudo apt install iw  (or "
              f"iwconfig {args.iface} mode Monitor)")


def main():
    p = argparse.ArgumentParser(
        description="WhereDaFlock Flock Cam signal simulator")
    sub = p.add_subparsers(dest="mode", required=True)

    r = sub.add_parser("radio", help="transmit fake Flock probe requests")
    r.add_argument("--iface", required=True, help="monitor-mode interface")
    r.add_argument("--cams", type=int, default=1, help="virtual cameras")
    r.add_argument("--tier", type=int, choices=[2, 3, 4], default=3,
                   help="2=OUI frame, 3=wildcard probe, 4=+vendor IE")
    r.add_argument("--dwell-ms", type=int, default=CAMERA_DWELL_MS,
                   help="ms between probe bursts per camera")
    r.add_argument("--hop", action="store_true", default=True,
                   help="hop channels ascending like real cameras")
    r.add_argument("--hop-ms", type=int, default=CHANNEL_HOP_MS)
    r.add_argument("--count", type=int, default=0, help="stop after N frames")
    r.set_defaults(func=radio_mode)

    pck = sub.add_parser("pcap", help="write synthetic frames to a pcap")
    pck.add_argument("--out", default="flock_sim.pcap")
    pck.add_argument("--oui", default=random.choice(TARGET_OUIS))
    pck.add_argument("--tier", type=int, choices=[2, 3, 4], default=4)
    pck.add_argument("--count", type=int, default=50)
    pck.set_defaults(func=pcap_mode)

    rep = sub.add_parser("replay", help="stream detection NDJSON (no radio)")
    rep.add_argument("--rate", type=float, default=1.0, help="lines/sec")
    rep.add_argument("--tier", type=int, choices=[0, 1, 2, 3, 4], default=4)
    rep.add_argument("--macs", type=int, default=3)
    rep.add_argument("--count", type=int, default=0)
    rep.set_defaults(func=replay_mode)

    c = sub.add_parser("check", help="verify an interface for injection")
    c.add_argument("--iface", required=True)
    c.set_defaults(func=check_mode)

    args = p.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()