#!/usr/bin/env python3
"""
WhereDaFlock - RF Spectrum & Police Radio Scanner Bridge
--------------------------------------------------------
Bridges external Software Defined Radios (SDR) and sub-GHz receivers
(RTL-SDR, HackRF, OP25 P25 trunking, rtl_433, CC1101 serial modules)
directly to the WhereDaFlock M5StickC / ESP32 detector over USB/Serial.

When police trunking talkgroups, emergency frequencies, or drone RF
signals are detected by the SDR, this bridge sends formatted alerts
to the WhereDaFlock hardware to trigger the tactical HUD alert, police
tone warble, LED flash, and telemetry broadcast.

Usage:
  # Simulated test signals:
  python3 tools/rf_bridge.py --port /dev/ttyACM0 --simulate

  # Monitor stdin stream (piped from rtl_fm, op25, or rtl_433):
  op25_rx | python3 tools/rf_bridge.py --port /dev/ttyACM0 --pipe

  # Manual frequency alert injection:
  python3 tools/rf_bridge.py --port /dev/ttyACM0 --inject --freq 851.250 --proto P25 --rssi -65 --desc "PD DISPATCH CH1"
"""

import argparse
import json
import sys
import time
import random

try:
    import serial
except ImportError:
    serial = None

# Common Law Enforcement & Public Safety Frequency Presets
POLICE_FREQUENCY_PRESETS = [
    {"freq": 851.250, "proto": "P25 Phase 2", "rssi": -62, "desc": "Metro Police Dispatch"},
    {"freq": 852.125, "proto": "P25 Phase 1", "rssi": -68, "desc": "County Sheriff Tactical"},
    {"freq": 770.4375, "proto": "P25 Trunked", "rssi": -58, "desc": "State Police Highway Net"},
    {"freq": 460.025, "proto": "UHF Analog/DMR", "rssi": -72, "desc": "City PD Dispatch Ch 1"},
    {"freq": 155.475, "proto": "VHF National", "rssi": -75, "desc": "Law Enforcement Mutual Aid"},
    {"freq": 915.000, "proto": "FHSS / ISM", "rssi": -55, "desc": "UAV Tactical Link"},
]


def send_radio_alert(ser, freq: float, proto: str, rssi: int, desc: str, use_json: bool = False):
    """Format and transmit an RF alert to WhereDaFlock over serial."""
    if use_json:
        payload = {
            "cmd": "radio_alert",
            "freq": round(freq, 4),
            "proto": proto,
            "rssi": rssi,
            "desc": desc
        }
        line = json.dumps(payload) + "\n"
    else:
        # High-efficiency plain text command parsed directly by WhereDaFlock firmware
        proto_clean = proto.replace(" ", "_")
        desc_clean = desc.replace("\n", "").replace("\r", "")
        line = f"RADIO {freq:.4f} {proto_clean} {rssi} {desc_clean}\n"

    print(f"[RF BRIDGE -> ESP32] {line.strip()}")
    if ser:
        ser.write(line.encode("utf-8"))
        ser.flush()


def run_simulation(ser, interval: float = 4.0, count: int = 10):
    print(f"[*] Starting RF Police Radio simulation ({count} transmissions, interval={interval}s)...")
    for i in range(count):
        preset = random.choice(POLICE_FREQUENCY_PRESETS)
        rssi = preset["rssi"] + random.randint(-6, 6)
        send_radio_alert(ser, preset["freq"], preset["proto"], rssi, preset["desc"])
        time.sleep(interval)
    print("[*] Simulation complete.")


def run_pipe(ser):
    print("[*] Reading piped frequency alerts from stdin (format: <freq> <proto> <rssi> <desc>)...")
    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            parts = line.split(maxsplit=3)
            freq = float(parts[0])
            proto = parts[1] if len(parts) > 1 else "P25"
            rssi = int(parts[2]) if len(parts) > 2 else -70
            desc = parts[3] if len(parts) > 3 else "Police Radio"
            send_radio_alert(ser, freq, proto, rssi, desc)
        except Exception as err:
            print(f"[!] Error parsing line '{line}': {err}")


def main():
    parser = argparse.ArgumentParser(description="WhereDaFlock RF Spectrum & Police Radio Bridge")
    parser.add_argument("--port", default="/dev/ttyACM0", help="Serial port to ESP32 (e.g. /dev/ttyACM0, /dev/ttyUSB0)")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate (default: 115200)")
    parser.add_argument("--simulate", action="store_true", help="Send simulated police radio transmissions")
    parser.add_argument("--count", type=int, default=5, help="Simulation alert count")
    parser.add_argument("--interval", type=float, default=3.5, help="Simulation interval (seconds)")
    parser.add_argument("--pipe", action="store_true", help="Read stream from stdin and bridge to serial")
    parser.add_argument("--inject", action="store_true", help="Inject a single radio alert")
    parser.add_argument("--freq", type=float, default=851.250, help="Radio frequency in MHz (default: 851.250)")
    parser.add_argument("--proto", default="P25", help="Protocol / modulation (default: P25)")
    parser.add_argument("--rssi", type=int, default=-65, help="Received signal strength (default: -65)")
    parser.add_argument("--desc", default="Metro Police Dispatch", help="Signal description / talkgroup")
    parser.add_argument("--dry-run", action="store_true", help="Print alerts without opening serial port")

    args = parser.parse_args()

    ser = None
    if not args.dry_run:
        if serial is None:
            print("[!] pyserial is not installed. Run 'pip install pyserial' or use --dry-run.")
            sys.exit(1)
        try:
            ser = serial.Serial(args.port, args.baud, timeout=1.0)
            print(f"[+] Connected to WhereDaFlock on {args.port} @ {args.baud} baud")
            time.sleep(1.5)  # Allow DTR settle
        except Exception as err:
            print(f"[!] Could not open serial port {args.port}: {err}")
            print("[*] Falling back to dry-run mode.")
            ser = None

    if args.inject:
        send_radio_alert(ser, args.freq, args.proto, args.rssi, args.desc)
    elif args.simulate:
        run_simulation(ser, interval=args.interval, count=args.count)
    elif args.pipe:
        run_pipe(ser)
    else:
        run_simulation(ser, interval=args.interval, count=args.count)


if __name__ == "__main__":
    main()
