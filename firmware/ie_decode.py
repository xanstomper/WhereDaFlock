#!/usr/bin/env python3
"""
WhereDaFlock - deep 802.11 Information-Element decoder (educational).

Decodes common 802.11 Information Elements beyond a name+length list: security
(RSN), HT/VHT capabilities, channel/country, capabilities bits, and rates.

All functions take the raw value bytes of an IE and return a readable string.
Note that RSN and HT fields are big-endian within the payload.

Used by packet_analyzer.py via decode_ie(tag, value).
"""

import struct

# Cipher suite OIDs (3 bytes after 00-0F-AC)
CIPHERS = {
    1: "WEP40", 2: "TKIP", 4: "CCMP", 5: "WEP104", 6: "AES-CMAC",
    7: "GCMP", 8: "GCMP-256", 9: "CCMP-256", 10: "BIP-GMAC-128",
    11: "BIP-GMAC-256", 12: "BIP-CMAC-256",
}
AKMS = {
    1: "802.1X", 2: "PSK", 3: "FT/802.1X", 4: "FT/PSK", 5: "802.1X-SHA256",
    6: "PSK-SHA256", 7: "TDLS", 8: "SAE", 9: "FT/SAE", 11: "OWE",
    13: "FT/PSK-SHA384", 14: "FT/802.1X-SHA384",
}

# A few common HT channel widths
HT_CW = {0: "20MHz", 1: "20/40MHz"}


def _hex_oui(oid: bytes) -> str:
    """Decode an 802.11 security suite selector: [0x00, 0x0F, 0xAC, id]."""
    if len(oid) == 4 and oid[:3] == b"\x00\x0f\xac":
        return CIPHERS.get(oid[3], f"OUI-AC#{oid[3]}")
    return f"OUI {oid.hex()}"


def _akm_oui(oid: bytes) -> str:
    if len(oid) == 4 and oid[:3] == b"\x00\x0f\xac":
        return AKMS.get(oid[3], f"AKM#{oid[3]}")
    return f"AKM-OUI {oid.hex()}"


def decode_rsn(val: bytes) -> str:
    """RSN (tag 48): version(2) group_cipher(4) pairwise_count(2) pairwises... akms..."""
    if len(val) < 8:
        return f"(truncated {len(val)}b)"
    version = struct.unpack(">H", val[0:2])[0]
    group = _hex_oui(val[2:6])
    pc = struct.unpack(">H", val[6:8])[0]
    pos = 8
    pairwises = []
    for _ in range(min(pc, 8)):
        if pos + 4 > len(val):
            break
        pairwises.append(_hex_oui(val[pos:pos + 4]))
        pos += 4
    akms = []
    if pos + 2 <= len(val):
        ac = struct.unpack(">H", val[pos:pos + 2])[0]
        pos += 2
        for _ in range(min(ac, 8)):
            if pos + 4 > len(val):
                break
            akms.append(_akm_oui(val[pos:pos + 4]))
            pos += 4
    caps = "no-caps"
    if pos + 2 <= len(val):
        caps = f"caps=0x{val[pos]:02X}{val[pos+1]:02X}"
    return (f"ver={version} group={group} pwises={','.join(pairwises) or '-'} "
            f"akms={','.join(akms) or '-'} {caps}")


def decode_ht_caps(val: bytes) -> str:
    """HT Capabilities (tag 45): 2-byte cap word then various."""
    if len(val) < 2:
        return "(short)"
    caps = struct.unpack("<H", val[0:2])[0]
    cw = caps & 0x3
    mcs = "n/a"
    if len(val) >= 5:
        mcs_bytes = val[3:5]  # supported MCS set (first 2 bytes of 16), bit0 = MCS0-7
        hi = int.from_bytes(mcs_bytes, "little")
        mcs = "MCS0-7" if (hi & 0xFF) else f"MCS0x{hi:04x}"
    return f"channel_width={HT_CW.get(cw, cw)} {mcs} short_gi={'yes' if caps & (1<<5) else 'no'}"


def decode_ht_info(val: bytes) -> str:
    """HT Operation (tag 61): primary channel byte0, then extras."""
    if not val:
        return "(empty)"
    primary = val[0]
    mode = (val[1] >> 4) & 0x3 if len(val) > 1 else 0  # secondary channel offset
    return f"primary_ch={primary} secondary_offset={mode}"


def decode_country(val: bytes) -> str:
    """Country (tag 7): 2-char code + first reg triplet."""
    if len(val) < 3:
        return ""
    code = val[0:2].decode("ascii", "replace")
    env = val[2]
    return f"{code} (env={env if 32 <= env < 127 else chr(env)})"


def decode_vht_caps(val: bytes) -> str:
    if len(val) < 2:
        return "(short)"
    caps = struct.unpack("<I", val[0:4])[0] if len(val) >= 4 else struct.unpack("<H", val[0:2])[0]
    support80 = bool(caps & 0x1)
    support160 = bool(caps & 0x2)
    return f"vht_80MHz={support80} vht_160MHz={support160}"


def decode_rates(val: bytes, extended=False) -> str:
    rates = []
    for b in val:
        if extended and (b & 0x80) == 0x80:  # extended rate flag (0x80) in Basic set
            continue
        rate = (b & 0x7F) / 2.0
        rates.append((rate, "B" if b & 0x80 else ""))
    return " ".join(f"{r}{s}" for r, s in rates) or "(none)"


def decode_elements_int(val: bytes) -> str:
    """Extended Capabilities (tag 127) - first byte bit summary."""
    if not val:
        return ""
    b0 = val[0]
    out = []
    if b0 & 0x01: out.append("20/40BSS-Coen")
    if b0 & 0x02: out.append("20/40BSS-Intolerant")
    if b0 & 0x08: out.append("SSID-List")
    return ", ".join(out) or f"byte0=0x{b0:02X}"


# Tag -> (name, deep decoder)
DEEP = {
    1: ("Supported Rates", lambda v: decode_rates(v)),
    48: ("RSN/security", decode_rsn),
    45: ("HT Capabilities", decode_ht_caps),
    61: ("HT Operation", decode_ht_info),
    7: ("Country", decode_country),
    191: ("VHT Capabilities", decode_vht_caps),
    50: ("Extended Rates", lambda v: decode_rates(v, extended=True)),
    127: ("Extended Capabilities", decode_elements_int),
}


def decode_ie(tag, value):
    """Return a (canonical_name, description) pair for any IE value."""
    if tag in DEEP:
        name, fn = DEEP[tag]
        try:
            return name, fn(bytes(value))
        except Exception:
            return name, f"({len(value)}b)"
    return None, None