#!/usr/bin/env python3
"""
WhereDaFlock - PostGIS SQL Seeder Generator
Converts Backend/data/cameras.json into batched PostgreSQL/PostGIS INSERT statements.
"""

import json
import os
import sys

def escape_sql(val):
    if val is None:
        return "NULL"
    return "'" + str(val).replace("'", "''") + "'"

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    backend_dir = os.path.dirname(script_dir)
    json_path = os.path.join(backend_dir, "data", "cameras.json")
    out_sql_path = os.path.join(backend_dir, "migrations", "002_seed_cameras.sql")

    if not os.path.exists(json_path):
        print(f"[!] JSON seed not found at {json_path}")
        sys.exit(1)

    with open(json_path, "r", encoding="utf-8") as f:
        cameras = json.load(f)

    print(f"Generating PostGIS seed SQL for {len(cameras)} cameras...")

    chunk_size = 1000
    with open(out_sql_path, "w", encoding="utf-8") as out:
        out.write("-- WhereDaFlock Pre-Seeded Camera Surveillance Nodes\n")
        out.write("BEGIN;\n\n")

        for i in range(0, len(cameras), chunk_size):
            chunk = cameras[i:i+chunk_size]
            values = []
            for cam in chunk:
                cid = escape_sql(cam.get("id"))
                ctype = escape_sql(cam.get("type", "Flock Safety Camera"))
                lat = float(cam.get("coordinate", {}).get("latitude", 0.0))
                lon = float(cam.get("coordinate", {}).get("longitude", 0.0))
                addr = escape_sql(cam.get("address", ""))
                owner = escape_sql(cam.get("owner", "Flock Safety"))
                conf = float(cam.get("confidence", 95.0))
                is_conf = "TRUE" if cam.get("isConfirmed", True) else "FALSE"
                src_name = escape_sql(cam.get("source", {}).get("name", "DeFlock DB"))
                src_rel = float(cam.get("source", {}).get("reliability", 0.95))

                geom_expr = f"ST_SetSRID(ST_MakePoint({lon:.6f}, {lat:.6f}), 4326)"
                val = f"({cid}, {ctype}, {geom_expr}, {addr}, {owner}, {conf}, {is_conf}, {src_name}, {src_rel})"
                values.append(val)

            out.write("INSERT INTO cameras (id, type, geom, address, owner, confidence, is_confirmed, source_name, source_reliability)\n")
            out.write("VALUES\n  " + ",\n  ".join(values) + "\n")
            out.write("ON CONFLICT (id) DO NOTHING;\n\n")

        out.write("COMMIT;\n")

    print(f"[✓] Successfully generated PostGIS SQL seed: {out_sql_path} ({os.path.getsize(out_sql_path):,} bytes)")

if __name__ == "__main__":
    main()
