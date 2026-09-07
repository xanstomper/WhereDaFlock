# WhereDaFlock on M5Stack (M5StickC, M5StickC Plus, Core, Atom, StampS3)

WhereDaFlock's firmware targets **any ESP32** board. On M5Stack devices it uses
the [M5Unified](https://github.com/m5stack/M5Unified) hardware-abstraction
library, which auto-detects each board's LED, buzzer, buttons, and display. That
means one firmware source runs on the whole M5Stack family.

## Why M5Unified

The detector sketch only touches LEDs, the buzzer, buttons, and (optionally) the
screen. M5Unified turns those into board-neutral calls (`M5.Display`,
`M5.Speaker`, `M5.BtnA`, `wdf_hal::…`), so it works across:

- **M5StickC** and **M5StickC Plus / Plus 1.1** (ESP32-PICO-D4)
- **M5Stack Core / Core2 / CoreS3** (larger M5Stack modules)
- **M5Atom, M5StampS3, M5Capsule, M5Paper, M5Tough**, and more
- any generic ESP32 (XIAO, DevKit, custom) via the raw-GPIO fallback

`firmware/src/hal.h` is the abstraction seam: it compiles either the M5 pathway
(`WDF_USE_M5UNIFIED`) or the raw-GPIO pathway.

## Your device: M5StickC Plus 1.1

The M5StickC **Plus 1.1** is an ESP32-PICO-D4 stick with a 1.14" 135×240 TFT,
a passive **buzzer**, red **LED on GPIO 10**, power button (GPIO 37), and a Grove
port. Build the detector for it with:

```bash
cd firmware
pio run -e m5stickc_plus -t upload        # WiFi detector
pio run -e m5stickc_plus_ble -t upload    # BLE beacon scanner
pio device monitor
```

Because it uses WiFi on the same radio, flashing and monitoring are over USB-C.

## Build envs

| Env | Board | Purpose |
|-----|-------|---------|
| `m5stickc_plus` | M5StickC Plus 1.1 / 1.2 | WiFi detector (your device) |
| `m5stickc_plus_ble` | M5StickC Plus | BLE beacon scanner |
| `m5stickc` | M5StickC (original) | WiFi detector |
| `m5stack_cores3` | M5Stack CoreS3 | WiFi detector |
| `m5stack_stamps3` | M5Stack StampS3 | WiFi detector |
| `m5stack_generic` | M5Stack Core2 / any | override `board=...` as needed |

All M5 envs pull in `m5stack/M5Unified` and set `-D WDF_USE_M5UNIFIED=1`.
The BLE envs additionally pull in `ArduinoJson`.

## Hardware behavior on M5

- **LED** — blinks the M5StickC red LED (GPIO 10) on detection; M5Unified also
  flashes the display backlight.
- **Buzzer** — plays the per-tier chirps on the M5StickC Plus passive buzzer via
  `M5.Speaker` (also works on Core* / speaker-equipped boards).
- **Button** — `wdf_hal::anyButtonPressed()` reads `M5.BtnA`, ready for future
  button actions.
- **Display** — `wdf_hal::displayStatus(...)` shows a status line (scanning /
  detected count) on the stick's screen.
- No display/buzzer is required: those calls are no-ops on boards without them.

## Flash / partition note

The M5StickC Plus is a 4 MB ESP32-PICO (no PSRAM). The detector fits (~20% RAM /
~75% of the 1.25 MB app partition). If you add heavy features, watch the Flash
number in the build output; a 4 MB `partitions.csv` (`3M app / 1.5M SPIFFS`) is
included if you want more app space.

## Adding another M5Stack board

Add an env and set the correct `board` (from `espressif32` PlatformIO boards)
with the same `WDF_USE_M5UNIFIED=1` flag + M5Unified `lib_dep`. M5Unified
handles the peripherals:

```ini
[env:my_other_m5]
platform = espressif32@6.9.0
board = m5stack-core2          ; or m5stack-atom, m5stack-cores3, ...
framework = arduino
build_src_filter = +<WhereDaFlock_scanner.cpp> +<src/>
build_flags = -D WDF_USE_M5UNIFIED=1
lib_deps = m5stack/M5Unified@^0.2.0
```