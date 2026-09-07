# WhereDaFlock - Unified Build & Development Targets
#
# Usage:
#   make test          # run firmware detection test suites (WiFi + BLE + Packet Analyzer)
#   make backend       # run the Go spatial API server locally
#   make backend-test  # run Go spatial engine unit tests
#   make docker-up     # launch Go backend + PostGIS via Docker Compose
#   make dashboard     # run the local Python dashboard
#   make seed          # re-ingest and seed camera database (15,000+ records)
#   make coreml        # export YOLOv8 detection model to CoreML format
#   make firmware      # compile ESP32 firmware with PlatformIO
#   make analyze       # show packet analyzer usage
#   make ci            # run all test suites locally

PY=python3
GO=go

.PHONY: test backend backend-test docker-up docker-down dashboard seed coreml firmware wifi ble emulator analyze ci help

test:
	$(PY) firmware/tests/test_detection.py
	$(PY) firmware/tests/test_ble_detection.py
	$(PY) firmware/tests/test_packet_analyzer.py
	$(PY) firmware/tests/test_signal_math.py

backend:
	cd Backend && $(GO) run ./cmd/server

backend-test:
	cd Backend && $(GO) test -v ./...

docker-up:
	docker compose -f Backend/docker-compose.yml up -d --build

docker-down:
	docker compose -f Backend/docker-compose.yml down

dashboard:
	cd api && $(PY) app.py

seed:
	$(PY) datasets/seed_cameras.py

coreml:
	$(PY) tools/export_coreml.py

wifi:
	cd firmware && pio run -e xiao_esp32s3

ble:
	cd firmware && pio run -e xiao_esp32s3_ble

firmware: wifi ble

emulator:
	cd tools/emulator && pio run 2>/dev/null || \
		echo "Build emulator with arduino-cli: compile --fqbn esp32:esp32:esp32 FlockCam_emulator.ino"

analyze:
	$(PY) firmware/packet_analyzer.py --help

ci: test backend-test
	$(PY) -c 'import json; d = json.load(open("WhereDaFlock/Resources/cameras.json")); assert len(d) > 1000; print(f"[✓] {len(d)} cameras validated.")'

help:
	@echo "Available targets:"
	@echo "  make test          - Run WiFi, BLE & packet analyzer test suites"
	@echo "  make backend       - Launch Go spatial server (:8080)"
	@echo "  make backend-test  - Run Go backend unit tests"
	@echo "  make docker-up     - Spin up PostGIS + Go backend via Docker"
	@echo "  make dashboard     - Run local Python research dashboard"
	@echo "  make seed          - Re-generate 15,000 camera JSON/GeoJSON dataset"
	@echo "  make coreml        - Run YOLOv8 CoreML model export"
	@echo "  make firmware      - Build ESP32 firmware (WiFi + BLE)"
	@echo "  make analyze       - Show packet analyzer usage"
	@echo "  make ci            - Run all local validation checks"