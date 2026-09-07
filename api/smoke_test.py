#!/usr/bin/env python3
"""
WhereDaFlock Dashboard Smoke Test
Verifies that the Flask dashboard imports cleanly and the IEEE OUI database loads.
"""
import sys

def main():
    strict = "--strict" in sys.argv
    try:
        import app as m
        assert len(m.OUI_DATABASE) > 0, "OUI DB empty"
        print(f"[ok] dashboard imports, {len(m.OUI_DATABASE)} OUI entries")
    except ModuleNotFoundError as e:
        print(f"[skip] dashboard dependencies missing ({e}); install api/requirements.txt")
        if strict:
            sys.exit(1)

if __name__ == "__main__":
    main()
