#!/usr/bin/env python3
"""
WhereDaFlock - Camera Dataset Seeder & DeFlock Converter.

Ingests field datasets (Pigvision, Penguin, Flock AP, Ext Battery, maximum_dots)
and optional live OpenStreetMap / DeFlock Overpass API records into unified
JSON / GeoJSON formats for the iOS app bundle and Go/PostGIS backend.

Usage:
    python3 datasets/seed_cameras.py [--limit 5000] [--osm-fetch]
"""

import argparse
import csv
import json
import os
import sys
from datetime import datetime, timezone
import math

def haversine_m(lat1, lon1, lat2, lon2):
    R = 6371000.0
    phi1, phi2 = math.radians(lat1), math.radians(lat2)
    dphi = math.radians(lat2 - lat1)
    dlambda = math.radians(lon2 - lon1)
    a = math.sin(dphi / 2.0)**2 + math.cos(phi1) * math.cos(phi2) * math.sin(dlambda / 2.0)**2
    return 2.0 * R * math.atan2(math.sqrt(a), math.sqrt(1.0 - a))

def parse_pigvision(path):
    cameras = []
    if not os.path.exists(path):
        return cameras
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        reader = csv.DictReader(f)
        for idx, row in enumerate(reader):
            coord_str = row.get("coordinates", "").strip()
            if not coord_str or "," not in coord_str:
                continue
            try:
                parts = coord_str.split(",")
                lat = float(parts[0].strip())
                lon = float(parts[1].strip())
                cam_type = "Flock Safety Camera"
                raw_type = row.get("type", "").lower()
                if "traffic" in raw_type:
                    cam_type = "Traffic Camera"
                elif "speed" in raw_type:
                    cam_type = "Speed Camera"
                elif "alpr" in raw_type:
                    cam_type = "ALPR Camera"

                cameras.append({
                    "id": f"pigvision-{idx+1:05d}",
                    "type": cam_type,
                    "lat": lat,
                    "lon": lon,
                    "address": row.get("info/comments", "") or row.get("name", "") or "Corridor ALPR",
                    "owner": "Flock Safety",
                    "confidence": 95.0,
                    "isConfirmed": True,
                    "source": "Pigvision ALPR Survey",
                })
            except Exception:
                continue
    return cameras

def parse_trilat_csv(path, prefix, cam_type="Flock Safety Camera", source_name="WiGLE / RF Survey"):
    cameras = []
    if not os.path.exists(path):
        return cameras
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        reader = csv.DictReader(f)
        for idx, row in enumerate(reader):
            try:
                lat = float(row.get("trilat", 0))
                lon = float(row.get("trilong", 0))
                if abs(lat) < 0.001 and abs(lon) < 0.001:
                    continue
                ssid = row.get("ssid", "Flock-Node")
                cameras.append({
                    "id": f"{prefix}-{idx+1:05d}",
                    "type": cam_type,
                    "lat": lat,
                    "lon": lon,
                    "address": f"RF Marker: {ssid}",
                    "owner": "Flock Safety",
                    "confidence": 92.0,
                    "isConfirmed": True,
                    "source": source_name,
                })
            except Exception:
                continue
    return cameras

def parse_maxdots(path):
    cameras = []
    if not os.path.exists(path):
        return cameras
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        reader = csv.reader(f)
        header = None
        for idx, row in enumerate(reader):
            if idx == 0:
                header = [h.lower() for h in row]
                continue
            if len(row) < 2:
                continue
            try:
                lon = float(row[0])
                lat = float(row[1])
                desc = row[3] if len(row) > 3 else "Municipal ALPR"
                cameras.append({
                    "id": f"maxdots-{idx:05d}",
                    "type": "ALPR Camera",
                    "lat": lat,
                    "lon": lon,
                    "address": desc,
                    "owner": "Law Enforcement",
                    "confidence": 90.0,
                    "isConfirmed": True,
                    "source": "Municipal Fixed ALPR Dataset",
                })
            except Exception:
                continue
    return cameras

def fetch_osm_deflock():
    """Fetch live OpenStreetMap ALPR / DeFlock nodes via Overpass API."""
    import urllib.request
    import urllib.parse

    print("[*] Querying OpenStreetMap Overpass API for ALPR surveillance nodes...")
    overpass_url = "https://overpass-api.de/api/interpreter"
    query = """
    [out:json][timeout:25];
    (
      node["man_made"="surveillance"]["surveillance:type"="ALPR"](24.0,-125.0,50.0,-66.0);
      node["camera:type"="alpr"](24.0,-125.0,50.0,-66.0);
    );
    out body 2000;
    """
    data = urllib.parse.urlencode({"data": query}).encode("utf-8")
    req = urllib.request.Request(overpass_url, data=data, headers={"User-Agent": "WhereDaFlock/1.0"})
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            result = json.loads(resp.read().decode("utf-8"))
            cameras = []
            for idx, el in enumerate(result.get("elements", [])):
                tags = el.get("tags", {})
                lat = el.get("lat")
                lon = el.get("lon")
                operator = tags.get("operator", tags.get("surveillance:operator", "Unknown Operator"))
                c_type = "Flock Safety Camera" if "flock" in operator.lower() else "ALPR Camera"
                cameras.append({
                    "id": f"osm-{el.get('id', idx)}",
                    "type": c_type,
                    "lat": lat,
                    "lon": lon,
                    "address": tags.get("name") or tags.get("description") or f"OSM Node {el.get('id')}",
                    "owner": operator,
                    "confidence": 96.0,
                    "isConfirmed": True,
                    "source": "OpenStreetMap / DeFlock Community",
                })
            print(f"[✓] Fetched {len(cameras)} nodes from OpenStreetMap Overpass")
            return cameras
    except Exception as exc:
        print(f"[!] Overpass API notice (offline/throttled): {exc}")
        return []

def deduplicate_spatial(cameras, min_dist_m=25.0):
    """Filter out spatial duplicates within min_dist_m."""
    unique = []
    for c in cameras:
        lat, lon = c["lat"], c["lon"]
        is_dupe = False
        for u in unique[-100:]: # check local recent neighborhood
            if haversine_m(lat, lon, u["lat"], u["lon"]) < min_dist_m:
                is_dupe = True
                break
        if not is_dupe:
            unique.append(c)
    return unique

def main():
    parser = argparse.ArgumentParser(description="WhereDaFlock Camera Seeder")
    parser.add_argument("--limit", type=int, default=15000, help="Max cameras to output (default: 15000)")
    parser.add_argument("--osm-fetch", action="store_true", help="Fetch live DeFlock ALPR nodes from OpenStreetMap Overpass API")
    args = parser.parse_args()

    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    datasets_dir = os.path.join(base_dir, "datasets")

    print("=== WhereDaFlock Surveillance Database Seeder ===")
    all_raw = []

    # 1. Pigvision
    pig_cams = parse_pigvision(os.path.join(datasets_dir, "Pigvision.csv"))
    print(f"Loaded {len(pig_cams)} cameras from Pigvision.csv")
    all_raw.extend(pig_cams)

    # 2. Penguin
    peng_cams = parse_trilat_csv(os.path.join(datasets_dir, "Penguin-___________20240530_111436.csv"),
                                 prefix="penguin", cam_type="Flock Safety Camera", source_name="Penguin ALPR Survey")
    print(f"Loaded {len(peng_cams)} cameras from Penguin CSV")
    all_raw.extend(peng_cams)

    # 3. Flock AP Beacons
    ap_cams = parse_trilat_csv(os.path.join(datasets_dir, "Flock-_______20240530_124303.csv"),
                              prefix="flock-ap", cam_type="Flock Safety Camera", source_name="Flock AP Survey")
    print(f"Loaded {len(ap_cams)} cameras from Flock AP CSV")
    all_raw.extend(ap_cams)

    # 4. External Battery Packs
    bat_cams = parse_trilat_csv(os.path.join(datasets_dir, "FS+Ext+Battery_20240530_105846.csv"),
                               prefix="fs-bat", cam_type="Flock Safety Camera", source_name="FS Ext Battery Survey")
    print(f"Loaded {len(bat_cams)} cameras from FS Ext Battery CSV")
    all_raw.extend(bat_cams)

    # 5. Maximum Dots
    max_cams = parse_maxdots(os.path.join(datasets_dir, "maximum_dots.csv"))
    print(f"Loaded {len(max_cams)} cameras from maximum_dots.csv")
    all_raw.extend(max_cams)

    # 6. Optional OSM fetch
    if args.osm_fetch:
        osm_cams = fetch_osm_deflock()
        all_raw.extend(osm_cams)

    print(f"\nTotal raw records collected: {len(all_raw)}")
    print("Deduplicating spatial nodes (within 25m)...")
    unique_cams = deduplicate_spatial(all_raw)[:args.limit]
    print(f"Retained {len(unique_cams)} high-confidence unique surveillance nodes.")

    # Convert to iOS Swift Camera model structure
    now_iso = datetime.now(timezone.utc).isoformat()
    ios_camera_records = []
    geojson_features = []

    for c in unique_cams:
        record = {
            "id": c["id"],
            "type": c["type"],
            "coordinate": {
                "latitude": round(c["lat"], 6),
                "longitude": round(c["lon"], 6)
            },
            "address": c["address"],
            "owner": c["owner"],
            "lastVerified": now_iso,
            "confidence": c["confidence"],
            "isConfirmed": c["isConfirmed"],
            "source": {
                "name": c["source"],
                "reliability": round(c["confidence"] / 100.0, 2),
                "lastUpdated": now_iso
            }
        }
        ios_camera_records.append(record)

        geojson_features.append({
            "type": "Feature",
            "geometry": {
                "type": "Point",
                "coordinates": [round(c["lon"], 6), round(c["lat"], 6)]
            },
            "properties": {
                "id": c["id"],
                "type": c["type"],
                "address": c["address"],
                "owner": c["owner"],
                "confidence": c["confidence"]
            }
        })

    # Save to iOS App Resources
    ios_res_dir = os.path.join(base_dir, "WhereDaFlock", "Resources")
    os.makedirs(ios_res_dir, exist_ok=True)
    json_path = os.path.join(ios_res_dir, "cameras.json")
    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(ios_camera_records, f, indent=2)
    print(f"[✓] Saved {len(ios_camera_records)} cameras to iOS bundle: {json_path}")

    geojson_path = os.path.join(ios_res_dir, "cameras.geojson")
    with open(geojson_path, "w", encoding="utf-8") as f:
        json.dump({"type": "FeatureCollection", "features": geojson_features}, f, indent=2)
    print(f"[✓] Saved GeoJSON layer to: {geojson_path}")

    # Also save to Backend data dir
    backend_data_dir = os.path.join(base_dir, "Backend", "data")
    os.makedirs(backend_data_dir, exist_ok=True)
    backend_json = os.path.join(backend_data_dir, "cameras.json")
    with open(backend_json, "w", encoding="utf-8") as f:
        json.dump(ios_camera_records, f)
    print(f"[✓] Synced to Backend database seed: {backend_json}")

if __name__ == "__main__":
    main()
