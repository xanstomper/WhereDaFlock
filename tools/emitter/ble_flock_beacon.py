#!/usr/bin/env python3
"""
WhereDaFlock - software Flock BLE beacon broadcaster (runs on the PC)
=====================================================================
Emits a REAL over-the-air Bluetooth Low Energy legacy (Bluetooth 4.2)
advertisement carrying the Flock Safety signals the WhereDaFlock detector looks for:

  * manufacturer Company Identifier 0x09C8  (decisive, weight 70)
  * advertised name "FS Ext Battery"         (supporting, weight 45)
  * Battery service UUID 0x180F              (supporting, weight 20)

Transmits on primary advertising channels 37, 38, 39 using raw HCI ADV_IND packets
compatible with Bluetooth 4.2 receivers (like the ESP32-PICO-D4 on M5StickC Plus).
Bluetooth uses a separate radio from WiFi; WiFi is never touched.
"""

import argparse
import os
import subprocess
import sys
import time

FLOCK_MFR_ID = 0x09C8
DEFAULT_NAME = "FS Ext Battery"
BATTERY_SERVICE_UUID = 0x180F


def check_sudo():
    if os.geteuid() != 0:
        print("[*] Raw HCI operations require root. Re-executing with sudo...")
        cmd = ["sudo", sys.executable] + sys.argv
        os.execvp("sudo", cmd)


def run_cmd(cmd_list, check=True):
    res = subprocess.run(cmd_list, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if check and res.returncode != 0:
        raise RuntimeError(f"Command failed: {' '.join(cmd_list)}\n{res.stderr}")
    return res


def build_hci_adv_data(name: str) -> str:
    # 31-byte max advertising packet:
    # 1. Flags (3 bytes): 02 01 06
    adv_bytes = [0x02, 0x01, 0x06]

    # 2. Complete 16-bit Service UUIDs: 0x180F (4 bytes)
    adv_bytes += [0x03, 0x03, 0x0F, 0x18]

    # 3. Manufacturer Specific Data: Company 0x09C8 (Flock) + 3-byte payload (7 bytes)
    adv_bytes += [0x06, 0xFF, 0xC8, 0x09, 0x03, 0x02, 0x01]

    # 4. Local Name (truncated to fit within 31 bytes total)
    max_name_len = 31 - len(adv_bytes) - 2  # reserve len + type bytes
    name_bytes = list(name.encode('utf-8')[:max_name_len])
    adv_bytes += [len(name_bytes) + 1, 0x09] + name_bytes

    total_len = len(adv_bytes)
    while len(adv_bytes) < 31:
        adv_bytes.append(0x00)

    hex_parts = [f"{total_len:02x}"] + [f"{b:02x}" for b in adv_bytes]
    return " ".join(hex_parts)


def start_advertising(adapter: str, name: str):
    # Disable advertising first
    run_cmd(["hcitool", "-i", adapter, "cmd", "0x08", "0x000a", "00"], check=False)

    # Set parameters: 100ms min (0x00a0), 120ms max (0x00c0), ADV_IND (00), channels 37,38,39 (0x07)
    run_cmd(["hcitool", "-i", adapter, "cmd", "0x08", "0x0006",
             "a0", "00", "c0", "00", "00", "00", "00", "00", "00", "00", "00", "00", "00", "07", "00"])

    # Set advertising data
    adv_hex = build_hci_adv_data(name)
    hex_tokens = adv_hex.split()
    run_cmd(["hcitool", "-i", adapter, "cmd", "0x08", "0x0008"] + hex_tokens)

    # Enable advertising
    run_cmd(["hcitool", "-i", adapter, "cmd", "0x08", "0x000a", "01"])


def stop_advertising(adapter: str):
    try:
        run_cmd(["hcitool", "-i", adapter, "cmd", "0x08", "0x000a", "00"], check=False)
    except Exception:
        pass


def main():
    parser = argparse.ArgumentParser(description="WhereDaFlock software Flock BLE beacon")
    parser.add_argument("--adapter", default="hci0", help="Bluetooth adapter (default: hci0)")
    parser.add_argument("--secs", type=float, default=0, help="Run for N seconds, then stop")
    parser.add_argument("--name", default=DEFAULT_NAME, help="Advertised device name")
    parser.add_argument("--stop", action="store_true", help="Stop advertising")
    args = parser.parse_args()

    check_sudo()

    if args.stop:
        stop_advertising(args.adapter)
        print(f"[*] Advertising stopped on {args.adapter}.")
        return

    was_bt_active = False
    status_res = subprocess.run(["systemctl", "is-active", "--quiet", "bluetooth"])
    if status_res.returncode == 0:
        was_bt_active = True
        print("[*] Temporarily pausing bluetooth.service to allow raw legacy HCI advertising...")
        subprocess.run(["systemctl", "mask", "--runtime", "bluetooth.service"])
        subprocess.run(["systemctl", "stop", "bluetooth"])

    try:
        run_cmd(["hciconfig", args.adapter, "up"])
        start_advertising(args.adapter, args.name)
        print(f"[*] BLE legacy advertising active on {args.adapter}:")
        print(f"    Name    : {args.name}")
        print(f"    MFR ID  : 0x{FLOCK_MFR_ID:04X} (Flock Safety)")
        print(f"    Service : 0x{BATTERY_SERVICE_UUID:04X} (Battery Service)")
        print(f"    Type    : ADV_IND (Legacy BT 4.2, channels 37/38/39)")

        if args.secs > 0:
            print(f"[*] Running for {args.secs:.0f} seconds (Ctrl-C to stop early)...")
            start_t = time.time()
            while time.time() - start_t < args.secs:
                time.sleep(1)
                run_cmd(["hcitool", "-i", args.adapter, "cmd", "0x08", "0x000a", "01"], check=False)
        else:
            print("[*] Running indefinitely (Ctrl-C to stop)...")
            while True:
                time.sleep(2)
                run_cmd(["hcitool", "-i", args.adapter, "cmd", "0x08", "0x000a", "01"], check=False)
    except KeyboardInterrupt:
        print("\n[*] Stopping advertiser...")
    finally:
        print("[*] Cleaning up BLE broadcast...")
        stop_advertising(args.adapter)
        if was_bt_active:
            print("[*] Restoring bluetooth.service...")
            subprocess.run(["systemctl", "unmask", "bluetooth.service"])
            subprocess.run(["systemctl", "start", "bluetooth"])
        print("[*] Done.")


if __name__ == "__main__":
    main()