# WhereDaFlock Dashboard

Real-time ingest web dashboard for the WhereDaFlock Flock detectors. It shows
detections from **both** the WiFi promiscuous detector and the BLE beacon
scanner, tags them with GPS, and exports CSV / KML (Google Earth).

This mirrors the "dashboard phase" of
[colonelpanichacks/flock-you](https://github.com/colonelpanichacks/flock-you).

---

## Features

- **Real-time monitoring** via WebSocket (Flask-SocketIO).
- **Dual-protocol ingest** — WiFi (`wifi_2_4ghz`) and BLE (`ble`) detections in
  one stream.
- **Serial ingest** — read NDJSON from an ESP32 detector over USB @ 115200.
- **HTTP feed** — `POST /api/detections` for external sources.
- **GPS** — USB NMEA puck (e.g. Ublox) or `gpsd` over TCP; tags each detection.
- **Export** — CSV and Google-Earth KML with lat/lon when GPS is available.

---

## Setup

```bash
cd api
pip install -r requirements.txt
python app.py
open http://localhost:5000
```

Requirements: Python 3.8+, and the deps in `requirements.txt` (Flask,
Flask-SocketIO, pyserial, eventlet).

---

## Connecting a detector

1. Flash either the WiFi detector (`WhereDaFlock_scanner.ino`) or the BLE
   scanner (`WhereDaFlock_ble.ino`) to an ESP32.
2. Plug the ESP32 in over USB (it prints one JSON line per detection @ 115200).
3. In the dashboard, pick the ESP32's serial port under **Sources** and click
   **Connect**.
4. Detections appear live. For BLE you can also use the emulator beacon
   (`tools/emulator/FlockCam_emulator.ino`) on a second ESP32 to test.

Without hardware, POST a JSON detection:

```bash
curl -X POST http://localhost:5000/api/detections \
  -H 'Content-Type: application/json' \
  -d '{"protocol":"ble","mac":"D4:E9:F4:B1:40:0C","rssi":-62,"method":"mfr_id","verdict":"FLOCK_LIKELY"}'
```

---

## API endpoints

| Method | Path | Purpose |
|--------|------|---------|
| GET | `/` | Dashboard UI |
| POST | `/api/detections` | Ingest a detection |
| GET | `/api/detections` | List all detections |
| POST | `/api/clear` | Clear session |
| GET | `/api/serial/ports` | List serial ports |
| POST | `/api/serial/connect` | Start reading a serial port |
| POST | `/api/serial/disconnect` | Stop reading a serial port |
| POST | `/api/gps/connect` | Connect GPS (`serial` or `gpsd`) |
| POST | `/api/gps/disconnect` | Disconnect GPS |
| GET | `/api/gps` | GPS status |
| GET | `/api/export/csv` | Download CSV |
| GET | `/api/export/kml` | Download Google-Earth KML |

---

## Detection record (normalized)

```json
{
  "ts": "2026-09-07T00:12:00Z",
  "mac": "D4:E9:F4:B1:40:0C",
  "name": "FS Ext Battery",
  "rssi": -62,
  "dist_m": 1.4,
  "conf": 70,
  "method": "mfr_id",
  "tier": null,
  "protocol": "ble",
  "mfr_id": "0x09C8",
  "verdict": "FLOCK_LIKELY",
  "channel": null,
  "frequency": null,
  "lat": 36.728,
  "lon": -79.865,
  "source": "serial"
}
```

WiFi detections populate `channel` / `frequency` and `mfr_id` becomes the OUI;
BLE detections populate `mfr_id` (0x09C8) and omit channel/frequency.

---

## Security note

The dashboard binds `0.0.0.0:5000` by default for LAN use. Put it behind a
reverse proxy with auth for anything beyond a trusted LAN, and change
`SECRET_KEY`. This is a research tool, not a hardened service.