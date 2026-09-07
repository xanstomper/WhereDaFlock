#!/usr/bin/env bash
# ==============================================================================
# WhereDaFlock - Universal ESP32 Firmware Flasher
# Supports: Seeed XIAO ESP32-S3, LilyGO T-Dongle-S3, ESP32-S3 DevKit
# ==============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(dirname "$SCRIPT_DIR")"
FIRMWARE_DIR="$REPO_DIR/firmware"

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
BOLD='\033[1m'
NC='\033[0m'

echo -e "${BOLD}${BLUE}╔══════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}${BLUE}║             WhereDaFlock - Hardware Firmware Flasher             ║${NC}"
echo -e "${BOLD}${BLUE}╚══════════════════════════════════════════════════════════════════╝${NC}"

# Detect OS
OS_NAME="$(uname -s)"
PORTS=()

if [[ "$OS_NAME" == "Darwin" ]]; then
    while IFS= read -r p; do
        [[ -n "$p" ]] && PORTS+=("$p")
    done < <(ls /dev/cu.usbmodem* /dev/cu.usbserial* 2>/dev/null || true)
else
    while IFS= read -r p; do
        [[ -n "$p" ]] && PORTS+=("$p")
    done < <(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || true)
fi

echo -e "\n${BOLD}1. Detecting Connected Hardware...${NC}"
if [ ${#PORTS[@]} -eq 0 ]; then
    echo -e "${YELLOW}[!] No serial devices detected.${NC}"
    echo -e "    Please plug in your ESP32-S3 board via USB-C data cable."
    echo -e "    ${BOLD}Tip:${NC} If not detected, hold down the ${BOLD}BOOT${NC} button,"
    echo -e "    tap ${BOLD}RESET${NC}, then release BOOT to force ROM bootloader mode."
    echo -e "\nEnter port manually (e.g. /dev/ttyACM0) or press Enter to retry:"
    read -r MANUAL_PORT
    if [ -n "$MANUAL_PORT" ]; then
        SELECTED_PORT="$MANUAL_PORT"
    else
        echo -e "${RED}[✗] Aborted: no device port specified.${NC}"
        exit 1
    fi
elif [ ${#PORTS[@]} -eq 1 ]; then
    SELECTED_PORT="${PORTS[0]}"
    echo -e "${GREEN}[✓] Detected device at: ${BOLD}$SELECTED_PORT${NC}"
else
    echo -e "${GREEN}[✓] Multiple devices detected:${NC}"
    for i in "${!PORTS[@]}"; do
        echo "  [$((i+1))] ${PORTS[$i]}"
    done
    echo -n "Select port [1-${#PORTS[@]}]: "
    read -r PORT_IDX
    SELECTED_PORT="${PORTS[$((PORT_IDX-1))]}"
fi

echo -e "\n${BOLD}2. Select Firmware Target:${NC}"
echo "  [1] Seeed Studio XIAO ESP32-S3 (WiFi 2.4GHz Promiscuous Scanner) [Default]"
echo "  [2] LilyGO T-Dongle-S3 (with onboard TFT Display & APA102 LED)"
echo "  [3] Generic ESP32-S3 DevKit / NodeMCU"
echo "  [4] WhereDaFlock BLE Beacon Scanner (Passive 0x09C8 Detector)"
echo -n "Enter target [1-4, default=1]: "
read -r TARGET_CHOICE

TARGET_CHOICE="${TARGET_CHOICE:-1}"
ENV_NAME="xiao_esp32s3"

case "$TARGET_CHOICE" in
    1) ENV_NAME="xiao_esp32s3" ;;
    2) ENV_NAME="lilygo_t_dongle_s3" ;;
    3) ENV_NAME="generic_esp32s3" ;;
    4) ENV_NAME="xiao_esp32s3_ble" ;;
    *) ENV_NAME="xiao_esp32s3" ;;
esac

echo -e "\n${BOLD}3. Building and Flashing Environment: ${BLUE}$ENV_NAME${NC} to ${GREEN}$SELECTED_PORT${NC}..."

# Check toolchain availability
if command -v pio &>/dev/null; then
    echo -e "${GREEN}[✓] Using PlatformIO to compile and upload...${NC}"
    cd "$FIRMWARE_DIR"
    pio run -e "$ENV_NAME" -t upload --upload-port "$SELECTED_PORT"
    echo -e "\n${GREEN}[✓] Firmware successfully flashed!${NC}"
    echo -e "Starting serial monitor at 115200 baud (Press Ctrl+C to exit)..."
    pio device monitor -p "$SELECTED_PORT" -b 115200
elif command -v arduino-cli &>/dev/null; then
    echo -e "${GREEN}[✓] Using Arduino CLI to compile and upload...${NC}"
    cd "$REPO_DIR"
    SKETCH="firmware/WhereDaFlock_scanner.cpp"
    if [[ "$ENV_NAME" == *"ble"* ]]; then
        SKETCH="firmware/WhereDaFlock_ble.cpp"
    fi
    arduino-cli compile --fqbn esp32:esp32:esp32s3 "$SKETCH"
    arduino-cli upload -p "$SELECTED_PORT" --fqbn esp32:esp32:esp32s3 "$SKETCH"
    echo -e "\n${GREEN}[✓] Firmware successfully flashed!${NC}"
    arduino-cli monitor -p "$SELECTED_PORT" --config baudrate=115200
elif command -v esptool.py &>/dev/null; then
    echo -e "${GREEN}[✓] Using esptool.py to flash firmware binary...${NC}"
    BIN_FILE="$FIRMWARE_DIR/.pio/build/$ENV_NAME/firmware.bin"
    if [ ! -f "$BIN_FILE" ]; then
        echo -e "${RED}[✗] Precompiled binary not found at $BIN_FILE.${NC}"
        echo -e "Please install PlatformIO ('pip install platformio') to build from source."
        exit 1
    fi
    esptool.py --port "$SELECTED_PORT" --baud 921600 write_flash 0x10000 "$BIN_FILE"
    echo -e "\n${GREEN}[✓] Firmware successfully flashed!${NC}"
else
    echo -e "${YELLOW}[!] Neither PlatformIO nor Arduino-CLI was found in PATH.${NC}"
    echo -e "Installing PlatformIO via python3 venv..."
    python3 -m pip install --upgrade platformio || true
    if command -v pio &>/dev/null; then
        cd "$FIRMWARE_DIR"
        pio run -e "$ENV_NAME" -t upload --upload-port "$SELECTED_PORT"
    else
        echo -e "${RED}[✗] Toolchain missing. Run: pip install platformio${NC}"
        exit 1
    fi
fi
