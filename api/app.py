"""
WhereDaFlock Dashboard - real-time ingest for WiFi + BLE Flock detectors.

A small Flask + SocketIO web dashboard that:
  * reads newline-delimited JSON detections from a connected ESP32 over serial
    (both the WiFi promiscuous detector and the BLE beacon scanner), or from an
    HTTP feed
  * tags detections with GPS position (USB NMEA puck or gpsd)
  * shows them live in the browser via websocket
  * exports CSV and KML (Google Earth)

It mirrors the "dashboard phase" of colonelpanichacks/flock-you against
WhereDaFlock's dual-mode output.

Quick start:
    pip install -r requirements.txt
    python app.py
    open http://localhost:5000
"""

import glob
import json
import threading
import time
import queue
import os
from datetime import datetime, timezone

from flask import Flask, render_template, jsonify, request, Response
from flask_socketio import SocketIO

try:
    import serial
    import serial.tools.list_ports
    HAS_SERIAL = True
except Exception:
    HAS_SERIAL = False

# ---------------------------------------------------------------------------
# App + state
# ---------------------------------------------------------------------------
app = Flask(__name__)
app.config["SECRET_KEY"] = "wheredaflock-dev"   # change in production
socketio = SocketIO(app, cors_allowed_origins="*", async_mode="threading")

DETECTIONS = []            # all-time (session) detections, newest last
LOCK = threading.Lock()
IS_FILENAME_PREFIX = "wdf"

# ---------------------------------------------------------------------------
# Detection ingest
# ---------------------------------------------------------------------------
def _ingest(data: dict) -> dict:
    """Normalize a raw detection dict into the dashboard record."""
    now = datetime.now(timezone.utc)
    record = {
        "ts": data.get("ts") or now.isoformat(),
        "mac": data.get("mac") or data.get("mac_address") or "?",
        "name": data.get("name") or data.get("device_name") or "",
        "rssi": int(data.get("rssi", 0)),
        "dist_m": data.get("dist_m"),
        "conf": int(data.get("conf", 0)),
        "method": data.get("method") or data.get("detection_method") or "unknown",
        "tier": data.get("tier") or data.get("detection_tier"),
        "protocol": data.get("protocol") or "",
        "mfr_id": data.get("mfr_id") or data.get("oui") or "",
        "verdict": data.get("verdict") or "",
        "channel": data.get("channel"),
        "frequency": data.get("frequency"),
        "lat": data.get("lat") or data.get("gps", {}).get("latitude"),
        "lon": data.get("lon") or data.get("gps", {}).get("longitude"),
        "source": data.get("source") or "unknown",
    }
    with LOCK:
        DETECTIONS.append(record)
        cap = 2000
        if len(DETECTIONS) > cap:
            del DETECTIONS[: len(DETECTIONS) - cap]
    socketio.emit("detection", record)
    return record


@app.post("/api/detections")
def api_detect():
    data = request.get_json(force=True, silent=True) or {}
    record = _ingest(data)
    return jsonify({"status": "ok", "detection": record})


@app.get("/api/detections")
def api_list():
    with LOCK:
        return jsonify(DETECTIONS)


@app.post("/api/clear")
def api_clear():
    with LOCK:
        DETECTIONS.clear()
    socketio.emit("cleared", {})
    return jsonify({"status": "ok"})


# ---------------------------------------------------------------------------
# Serial ingest (WiFi and BLE ESP32 detectors)
# ---------------------------------------------------------------------------
def _serial_reader(port, baud=115200):
    """Read NDJSON lines from an ESP32 and ingest each detection."""
    ser = serial.Serial(port, baud, timeout=1)
    print(f"[wdf-dash] listening on {port} @ {baud}")
    try:
        while True:
            try:
                line = ser.readline().decode("utf-8", errors="ignore").strip()
            except Exception:
                continue
            if not line or not line.startswith("{"):
                continue
            try:
                data = json.loads(line)
            except json.JSONDecodeError:
                continue
            if data.get("event") in ("detection", "new", "update") or \
                    data.get("protocol") in ("wifi_2_4ghz", "ble"):
                _ingest({**data, "source": "serial"})
    except Exception as exc:
        print(f"[wdf-dash] serial loop ended: {exc}")

SERIAL_THREADS = {}


@app.get("/api/serial/ports")
def serial_ports():
    if not HAS_SERIAL:
        return jsonify({"error": "pyserial not installed"}), 400
    ports = []
    for p in serial.tools.list_ports.comports():
        ports.append({"device": p.device, "description": p.description})
    # include common raw tty entries as a fallback
    for path in glob.glob("/dev/ttyUSB*") + glob.glob("/dev/ttyACM*"):
        if path not in [p["device"] for p in ports]:
            ports.append({"device": path, "description": "(auto)"})
    return jsonify(ports)


@app.post("/api/serial/connect")
def serial_connect():
    port = (request.json or {}).get("port")
    if not port:
        return jsonify({"error": "port required"}), 400
    t = threading.Thread(target=_serial_reader, args=(port,), daemon=True)
    SERIAL_THREADS[port] = t
    t.start()
    return jsonify({"status": "ok", "message": f"listening on {port}"})


@app.post("/api/serial/disconnect")
def serial_disconnect():
    port = (request.json or {}).get("port")
    if port and port in SERIAL_THREADS:
        # thread exits when its read fails after disconnect; just drop ref
        SERIAL_THREADS.pop(port, None)
    return jsonify({"status": "ok"})


# ---------------------------------------------------------------------------
# GPS (USB NMEA puck or gpsd)
# ---------------------------------------------------------------------------
_GPS = {"lat": None, "lon": None, "source": "off"}


@app.post("/api/gps/connect")
def gps_connect():
    body = request.get_json(force=True) or {}
    source = body.get("source", "serial")
    if source == "gpsd":
        host = body.get("host", "localhost")
        port = int(body.get("port", 2947))
        socketio.start_background_task(_gpsd_loop, host, port)
        _GPS["source"] = "gpsd"
        return jsonify({"status": "ok"})
    port = body.get("port")
    if not port:
        return jsonify({"error": "port required"}), 400
    socketio.start_background_task(_nmea_loop, port)
    _GPS["source"] = "serial"
    return jsonify({"status": "ok"})


@app.post("/api/gps/disconnect")
def gps_disconnect():
    _GPS["source"] = "off"
    _GPS["lat"] = _GPS["lon"] = None
    return jsonify({"status": "ok"})


def _nmea_loop(port, baud=9600):
    ser = serial.Serial(port, baud, timeout=1)
    print(f"[wdf-dash] GPS NMEA on {port}")
    while _GPS["source"] == "serial":
        try:
            line = ser.readline().decode("ascii", errors="ignore")
        except Exception:
            break
        if line.startswith("$GPGGA"):
            parts = line.split(",")
            if len(parts) > 6 and parts[6] not in ("", "0"):
                lat = _nmea_to_dec(parts[2], parts[3])
                lon = _nmea_to_dec(parts[4], parts[5])
                _GPS["lat"], _GPS["lon"] = lat, lon


def _nmea_to_dec(raw, hemi):
    if not raw:
        return None
    try:
        deg = int(len(raw.split(".")[0]) - 2)
        d = float(raw[:deg])
        m = float(raw[deg:])
        dec = d + m / 60.0
        return -dec if hemi in ("S", "W") else dec
    except Exception:
        return None


def _gpsd_loop(host, port):
    import socket
    s = socket.create_connection((host, port), timeout=5)
    s.sendall(b'?WATCH={"enable":true,"json":true};\n')
    print(f"[wdf-dash] GPS via gpsd {host}:{port}")
    buf = b""
    while _GPS["source"] == "gpsd":
        try:
            buf += s.recv(4096)
        except Exception:
            break
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            try:
                obj = json.loads(line)
            except ValueError:
                continue
            if obj.get("class") == "TPV" and obj.get("mode") in (2, 3):
                _GPS["lat"] = obj.get("lat")
                _GPS["lon"] = obj.get("lon")


@app.get("/api/gps")
def gps_status():
    return jsonify(_GPS)


# ---------------------------------------------------------------------------
# Export
# ---------------------------------------------------------------------------
def _latest_gps(rec):
    return (rec.get("lat"), rec.get("lon")) or (_GPS["lat"], _GPS["lon"])


@app.get("/api/export/csv")
def export_csv():
    import csv, io
    buf = io.StringIO()
    w = csv.writer(buf)
    w.writerow(["timestamp", "mac", "name", "rssi", "dist_m", "conf", "method",
                "tier", "protocol", "mfr_id_oui", "verdict", "lat", "lon"])
    with LOCK:
        for r in DETECTIONS:
            lat, lon = _latest_gps(r)
            w.writerow([r["ts"], r["mac"], r["name"], r["rssi"], r["dist_m"],
                        r["conf"], r["method"], r["tier"], r["protocol"],
                        r["mfr_id"], r["verdict"], lat, lon])
    fname = f"{IS_FILENAME_PREFIX}_detections_{int(time.time())}.csv"
    return Response(buf.getvalue(), mimetype="text/csv",
                    headers={"Content-Disposition": f"attachment; filename={fname}"})


@app.get("/api/export/kml")
def export_kml():
    with LOCK:
        recs = [r for r in DETECTIONS if _latest_gps(r)[0] is not None]
    kml = ['<?xml version="1.0" encoding="UTF-8"?>',
           '<kml xmlns="http://www.opengis.net/kml/2.2"><Document>',
           f"<name>WhereDaFlock {IS_FILENAME_PREFIX}</name>"]
    for r in recs:
        lat, lon = _latest_gps(r)
        kml.append("<Placemark><name>%s</name><description>%s %sdBm</description>" % (
            r["mac"].replace("&", "&amp;"), r["method"], r["rssi"]))
        kml.append(f"<Point><coordinates>{lon},{lat},0</coordinates></Point></Placemark>")
    kml.append("</Document></kml>")
    fname = f"{IS_FILENAME_PREFIX}_detections_{int(time.time())}.kml"
    return Response("\n".join(kml), mimetype="application/vnd.google-earth.kml+xml",
                    headers={"Content-Disposition": f"attachment; filename={fname}"})


# ---------------------------------------------------------------------------
# UI
# ---------------------------------------------------------------------------
@app.get("/")
def index():
    return render_template("index.html")


if __name__ == "__main__":
    print("WhereDaFlock dashboard on http://localhost:5000")
    socketio.run(app, host="0.0.0.0", port=5000, debug=False)