# Flock Cam WiFi OUI collection

The WhereDaFlock WiFi detector matches the transmitter (and, at lower tiers,
receiver/BSSID) MAC against a curated set of **32 OUIs**. This document records
where the list came from, the current revision, and the change log.

> Source: **@NitekryDPaul / OrdoOuroboros** ([nite-oui-collection](https://github.com/nitekry/nite-oui-collection))
> as of the **2026-07-16** revision (31 active prefixes), plus one contributed
> by **DeFlockJoplin**.

---

## The active list (32)

```
70:c9:4e   3c:91:80   d8:f3:bc   80:30:49   b8:35:32
14:5a:fc   74:4c:a1   08:3a:88   9c:2f:9d   c0:35:32
94:08:53   e4:aa:ea   f4:6a:dd   e0:0a:f6   24:b2:b9
00:f4:8d   d0:39:57   e8:d0:fc   e0:4f:43   b8:1e:a4
70:08:94   58:8e:81   ec:1b:bd   3c:71:bf   58:00:e3
90:35:ea   5c:93:a2   64:6e:69   48:27:ea   a4:cf:12
14:b5:cd
82:6b:f2   ← contributed by DeFlockJoplin
```

31 active prefixes from NitekryDPaul + `82:6b:f2` from DeFlockJoplin = **32**.

---

## Where it lives in the code

| File | Role |
|------|------|
| `firmware/src/signatures.h` | `TARGET_OUIS[]` (source of truth, parsed to bytes at boot) |
| `firmware/host_scanner.py` | `TARGET_OUIS` (host-side mirror) |
| `firmware/tests/test_detection.py` | asserts the C and Python sets are identical |

Forthcoming `datasets/` usage: the esptext export / CI can diff the three to
catch drift.

---

## Change log

### 2026-07-16 sync (NitekryDPaul revision)
- **Removed** `f8:a2:d6` — demoted by NitekryDPaul (hits a Sony Media Player,
  not a Flock device).
- **Added** `e0:0a:f6` and `14:b5:cd`.

### DeFlockJoplin contribution
- **Added** `82:6b:f2` — contributed by DeFlockJoplin from drive testing in
  Joplin, MO. **Do not add a locally-administered MAC filter**: this OUI has
  bit 1 of the first octet set, so such a filter would silently drop it.

---

## Detection tiers that use the OUI

| Tier | Method | Gate |
|------|--------|------|
| 4 | `wifi_wildcard_probe_ie_sig` | OUI + wildcard SSID + IE fingerprint |
| 3 | `wifi_wildcard_probe` | OUI + wildcard SSID, IE unverified |
| 2 | `wifi_oui_addr2` | transmitter-side OUI on any frame |
| 1 | `wifi_oui_addr1` / `wifi_oui_addr3` | receiver / BSSID OUI (AP echo) |

The OUI alone is never enough for the highest tiers; it is combined with the
wildcard SSID and IE fingerprint to reach a confirmed detection.

---

## Full dataset methodology

For the complete methodology behind the flag list, region coverage, and the
drive-testing that produced these prefixes, see the upstream
[nite-oui-collection](https://github.com/nitekry/nite-oui-collection) repository
and DeFlockJoplin's published notes. WhereDaFlock ships the active list so the
detector functions out of the box.