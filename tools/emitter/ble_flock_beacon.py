#!/usr/bin/env python3
"""
WhereDaFlock - software Flock BLE beacon broadcaster (runs on the PC)
=====================================================================
Emits a REAL over-the-air Bluetooth Low Energy advertisement carrying the
Flock Safety signals the WhereDaFlock BLE detector looks for:

  * manufacturer Company Identifier 0x09C8  (decisive, weight 70)
  * advertised name "FS Ext Battery"         (supporting, weight 45)
  * Battery service UUID 0x180F              (supporting, weight 20)

Uses the native Linux BlueZ D-Bus LEAdvertisingManager1 interface on hci0,
with automatic fallback to bluetoothctl if dbus is unavailable.
Bluetooth uses a separate radio from WiFi; WiFi is never touched.
"""

import argparse
import sys
import time

FLOCK_MFR_ID = 0x09C8
DEFAULT_NAME = "FS Ext Battery"
BATTERY_SERVICE_UUID = "180F"


def run_dbus_advertiser(name: str, duration_sec: float):
    import dbus
    import dbus.service
    import dbus.mainloop.glib
    from gi.repository import GLib

    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    bus = dbus.SystemBus()

    class FlockAdvertisement(dbus.service.Object):
        def __init__(self, bus, index=0):
            self.path = f"/org/bluez/wdf/advertisement{index}"
            super().__init__(bus, self.path)

        def get_properties(self):
            return {
                'org.bluez.LEAdvertisement1': {
                    'Type': dbus.String('peripheral'),
                    'LocalName': dbus.String(name),
                    'ServiceUUIDs': dbus.Array([BATTERY_SERVICE_UUID], signature='s'),
                    'ManufacturerData': dbus.Dictionary({
                        dbus.UInt16(FLOCK_MFR_ID): dbus.Array([0x03, 0x02, 0x01], signature='y')
                    }, signature='qv'),
                    'Discoverable': dbus.Boolean(True),
                }
            }

        @dbus.service.method('org.freedesktop.DBus.Properties', in_signature='s', out_signature='a{sv}')
        def GetAll(self, interface):
            return self.get_properties().get(interface, {})

        @dbus.service.method('org.bluez.LEAdvertisement1', in_signature='', out_signature='')
        def Release(self):
            print("[*] Advertisement released by BlueZ.")

    adapter = bus.get_object('org.bluez', '/org/bluez/hci0')
    adv_mgr = dbus.Interface(adapter, 'org.bluez.LEAdvertisingManager1')
    adv = FlockAdvertisement(bus)
    loop = GLib.MainLoop()

    def on_registered():
        print(f"[*] BLE advertising active on hci0:")
        print(f"    Name    : {name}")
        print(f"    MFR ID  : 0x{FLOCK_MFR_ID:04X} (Flock Safety)")
        print(f"    Service : 0x{BATTERY_SERVICE_UUID}")
        if duration_sec > 0:
            print(f"[*] Running for {duration_sec:.0f} seconds (Ctrl-C to stop early)...")
            GLib.timeout_add_seconds(int(duration_sec), on_timeout)
        else:
            print("[*] Running indefinitely (Ctrl-C to stop)...")

    def on_timeout():
        print("[*] Duration elapsed, unregistering advertisement...")
        try:
            adv_mgr.UnregisterAdvertisement(adv.path)
        except Exception:
            pass
        loop.quit()

    def on_error(err):
        print(f"[-] Failed to register advertisement: {err}")
        loop.quit()

    adv_mgr.RegisterAdvertisement(adv.path, {}, reply_handler=on_registered, error_handler=on_error)
    try:
        loop.run()
    except KeyboardInterrupt:
        print("\n[*] Stopping advertiser...")
        try:
            adv_mgr.UnregisterAdvertisement(adv.path)
        except Exception:
            pass


def main():
    parser = argparse.ArgumentParser(description="WhereDaFlock software Flock BLE beacon")
    parser.add_argument("--secs", type=float, default=0, help="Run for N seconds, then stop")
    parser.add_argument("--name", default=DEFAULT_NAME, help="Advertised device name")
    parser.add_argument("--stop", action="store_true", help="Stop advertising")
    args = parser.parse_args()

    if args.stop:
        print("[*] Stopped.")
        return

    try:
        run_dbus_advertiser(args.name, args.secs)
    except ImportError:
        import subprocess
        print("[*] dbus/glib not available in this python environment; falling back to bluetoothctl...")
        p = subprocess.Popen(['bluetoothctl'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        cmd = f"menu advertise\nclear\nmanufacturer 0x09c8 0x03 0x02 0x01\nname '{args.name}'\nuuids 0x180f\nback\nadvertise on\n"
        p.stdin.write(cmd)
        p.stdin.flush()
        print(f"[*] Advertising via bluetoothctl. Name: {args.name}, MFR: 0x09C8")
        try:
            if args.secs > 0:
                time.sleep(args.secs)
            else:
                while True:
                    time.sleep(3600)
        except KeyboardInterrupt:
            pass
        finally:
            p.stdin.write("advertise off\nexit\n")
            p.stdin.flush()
            p.communicate(timeout=3)
            print("[*] Advertising stopped.")


if __name__ == "__main__":
    main()