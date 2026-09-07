# Contributing to WhereDaFlock

Thanks for helping improve WhereDaFlock. Please read and follow these
guidelines so changes stay consistent and reviewable.

---

## Code of conduct

Be respectful. This project is about privacy awareness and security research.
Do not use it to harass, target, or track individuals. Keep all contributions
passive and lawful in every jurisdiction in which they run.

---

## Project layout

```
WhereDaFlock/
├── WhereDaFlock/            # iOS SwiftUI app
├── firmware/                # ESP32 detector + host companion
│   ├── src/signatures.h     # OUI list + tiers (source of truth)
│   ├── WhereDaFlock_scanner.ino
│   ├── host_scanner.py      # must stay in sync with signatures.h
│   └── tests/test_detection.py
├── docs/                    # DETECTION-GUIDE, BUILD-FIRMWARE, HARDWARE, PROTOCOL
└── LICENSE                  # MIT
```

---

## Changing the signature set (OUI / tiers)

When you add or remove an OUI you **must** update all three places or the tests
fail:

1. `firmware/src/signatures.h` — `TARGET_OUIS[]`
2. `firmware/host_scanner.py` — `TARGET_OUIS`
3. `firmware/tests/test_detection.py` — add an assertion for the new OUI

Then run:

```bash
cd firmware
python3 tests/test_detection.py
```

The test verifies the C and Python OUI sets are identical, so keep them in sync
or CI (if added) will fail.

---

## Code style

- Firmware `.ino` / `.h`: C++17, 2-space indent, `IRAM_ATTR` on the sniffer
  callback, no `Serial`/`malloc` inside the WiFi callback.
- Python: PEP 8, type hints on new functions.
- Keep the receive-only principle: never add code that transmits or associates
  to a network.

---

## Tests

- Detection/tier tests live in `firmware/tests/test_detection.py` and run on
  any host (no hardware needed).
- When you change the host tool's behavior, update the tests.

---

## Pull request checklist

- [ ] `python3 firmware/tests/test_detection.py` passes.
- [ ] C and Python OUI lists are identical.
- [ ] No transmit/associate code added.
- [ ] Docs updated if behavior or the protocol changed (`docs/`).

---

## License

By contributing, you agree that your contributions are licensed under the MIT
License (see `LICENSE`).