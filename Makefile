# WhereDaFlock - helper targets
#
# Usage:
#   make test        # run both firmware Python test suites (host-side)
#   make firmware    # build both ESP32 sketches with PlatformIO
#   make dashboard   # run the Flask dashboard
#   make emulator    # build the Flock Cam BLE test beacon (transmits!)
#
# The ESP32 firmware is receive-only except the emulator (test-only).

PY=python3

.PHONY: test firmware wifi ble dashboard emulator analyze

test:
	$(PY) firmware/tests/test_detection.py
	$(PY) firmware/tests/test_ble_detection.py
	$(PY) firmware/tests/test_packet_analyzer.py

wifi:
	cd firmware && pio run -e xiao_esp32s3

ble:
	cd firmware && pio run -e xiao_esp32s3_ble

firmware: wifi ble

dashboard:
	cd api && $(PY) app.py

analyze:
	$(PY) firmware/packet_analyzer.py --help

emulator:
	cd tools/emulator && pio run 2>/dev/null || \
		echo "Build emulator with arduino-cli: compile --fqbn esp32:esp32:esp32 FlockCam_emulator.ino"

help:
	@echo "Targets: test, firmware, wifi, ble, dashboard, emulator"