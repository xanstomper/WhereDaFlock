#!/usr/bin/env python3
"""
WhereDaFlock - batch pcap aggregator (educational).

Aggregate multiple offline .pcap captures into a single report: per-transmitter
activity, frame-type histograms, RSSI/distance stats, randomized-MAC prevalence,
and (where possible) which transmitters look Flock-like by OUI.

Everything is passive analysis of local capture files.

Usage:
  python3 analyze_batch.py captures/*.pcap            # inspect all
  python3 analyze_batch.py a.pcap b.pcap --json out.json
  python3 analyze_batch.py *.pcap --html report.html
"""

import argparse
import glob
import json
import os
import sys
from collections import Counter


def load_frames(path, limit=200000):
    """Parse a pcap into list of wifi_to_dict() records (best-effort)."""
    from scapy.all import rdpcap
    frames = []
    for pkt in rdpcap(path):
        try:
            from scapy.layers.dot11 import Dot11
            if not pkt.haslayer(Dot11):
                continue
            import packet_analyzer
            raw = bytes(pkt)
            rssi = getattr(pkt, "dBm_AntSignal", None) or getattr(pkt, "signal", None)
            d = packet_analyzer.wifi_to_dict(raw, rssi=int(rssi) if rssi is not None else None)
            if d.get("ok"):
                frames.append(d)
            if len(frames) >= limit:
                break
        except Exception:
            continue
    return frames


def aggregate(files):
    """Return (per-file records, aggregate stats)."""
    from host_scanner import is_target_oui
    per_file = {}
    agg = {
        "files": [], "frames": 0, "transmitters": Counter(),
        "subtypes": Counter(), "randomized": 0, "flock_like": Counter(),
        "distances": [],
    }
    for path in files:
        frames = load_frames(path)
        rec = {
            "path": path, "frames": len(frames),
            "transmitters": sorted({f.get("add2") for f in frames if f.get("add2")}),
        }
        per_file[path] = rec
        agg["files"].append(path)
        agg["frames"] += len(frames)
        for f in frames:
            tx = f.get("add2")
            if tx:
                agg["transmitters"][tx] += 1
                if is_target_oui(tx):
                    agg["flock_like"][tx] += 1
            if f.get("transmitter_randomized"):
                agg["randomized"] += 1
            agg["subtypes"][(f.get("type"), f.get("subtype"))] += 1
            if "dist_m" in f:
                agg["distances"].append(f["dist_m"])
    return per_file, agg


def format_report(agg, per_file):
    lines = []
    lines.append("=" * 64)
    lines.append("WhereDaFlock batch pcap report")
    lines.append("=" * 64)
    lines.append(f"  files            : {len(agg['files'])}")
    lines.append(f"  total frames     : {agg['frames']}")
    lines.append(f"  unique transmitters: {len(agg['transmitters'])}")
    lines.append(f"  randomized addrs : {agg['randomized']} frames "
                 "(locally-administered bit set)")
    flock_total = sum(agg["flock_like"].values())
    lines.append(f"  Flock-OUI frames : {flock_total} "
                 f"across {len(agg['flock_like'])} transmitter(s)")
    lines.append("  frame types:")
    for (t, s), n in agg["subtypes"].most_common(12):
        lines.append(f"    {t:<11} / {s:<22} n={n}")
    if agg["distances"]:
        lines.append(f"  est. distance    : min={min(agg['distances']):.1f}m "
                     f"max={max(agg['distances']):.1f}m "
                     f"(n={len(agg['distances'])})")
    lines.append("-" * 64)
    lines.append("Per file:")
    for path, rec in per_file.items():
        lines.append(f"  {os.path.basename(path):<30} frames={rec['frames']:<6} "
                     f"tx={len(rec['transmitters'])}")
    lines.append("=" * 64)
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser(description="aggregate multiple pcaps")
    ap.add_argument("pcaps", nargs="+", help=".pcap files or glob")
    ap.add_argument("--json", metavar="FILE", help="write aggregate JSON")
    ap.add_argument("--html", metavar="FILE", help="write an HTML report")
    args = ap.parse_args()

    files = []
    for p in args.pcaps:
        files += glob.glob(p) if glob.has_magic(p) else [p]
    if not files:
        sys.exit("no pcap files matched")

    per_file, agg = aggregate(files)
    print(format_report(agg, per_file))

    if args.json:
        with open(args.json, "w") as f:
            json.dump({
                "frames": agg["frames"],
                "transmitters": sorted(agg["transmitters"].items()),
                "subtypes": sorted([list(k) + [v] for k, v in agg["subtypes"].items()]),
                "flock_like": sorted(agg["flock_like"].items()),
            }, f, indent=2)
        print(f"\nWrote {args.json}")

    if args.html:
        # Build a minimal self-contained HTML report.
        top = agg["transmitters"].most_common(50)
        rows = "".join(
            f"<tr><td>{mac}</td><td>{n}</td><td>{'YES' if flock.get(mac) else ''}</td></tr>"
            for mac, n in top)
        flock = dict(agg["flock_like"])
        html = f"""<!doctype html><html><head><meta charset=utf-8>
<title>WhereDaFlock batch report</title>
<style>body{{font-family:sans-serif;background:#0d1117;color:#e6edf3;padding:20px}}
table{{border-collapse:collapse;width:100%;font-size:13px}}
td,th{{border:1px solid #30363d;padding:6px;text-align:left}}
th{{background:#161b22}}</style></head><body>
<h2>WhereDaFlock batch report</h2>
<p>{agg['frames']} frames, {len(agg['transmitters'])} transmitters, {agg['randomized']} randomized.</p>
<h3>Top transmitters</h3><table><tr><th>MAC</th><th>frames</th><th>Flock-OUI</th></tr>{rows}</table>
</body></html>"""
        with open(args.html, "w") as f:
            f.write(html)
        print(f"\nWrote {args.html}")


if __name__ == "__main__":
    main()