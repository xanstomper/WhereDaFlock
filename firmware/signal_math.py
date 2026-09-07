#!/usr/bin/env python3
"""
WhereDaFlock - signal math (educational companion).

RSSI → distance estimation and multilateration ("triangulation by signal
strength") for radio research. Everything here is applied to positions you
already know (your own measurements of a transmitter in range); it is math, not
a transmitter.

Functions
---------
estimate_distance(rssi, tx_power=-59, n=2.0)   RSSI → meters (log-distance model)
rssi_to_distance_rough(rssi)                    typical dBm→m lookup table
multilaterate(anchors)                          least-squares position from (x,y,d)
is_locally_administered(mac)                    bit 1 of first octet (randomized)
is_multicast(mac)                               bit 0 of first octet
mac_randomization_label(mac)                    human readout of a MAC's bits
"""

import math


def estimate_distance(rssi, tx_power=-59, n=2.0):
    """
    Convert RSSI (dBm) to distance in meters using the log-normal path-loss
    model:

        RSSI = TxPower - 10*n*log10(d)

    which inverts to:

        d = 10 ** ((TxPower - RSSI) / (10*n))

    - tx_power : expected RSSI at 1 meter (reference). Common BLE/2.4GHz value.
    - n        : path-loss exponent. 2.0 = free space, 2.0-4.0 urban/indoor.

    Returns meters.
    """
    if rssi == 0:
        return -1.0
    if rssi > tx_power:  # too close / noise; clamp so we don't go < 1 m
        return 0.5
    return round(10 ** ((tx_power - rssi) / (10.0 * n)), 3)


# Typical rough mapping for a 2.4 GHz beacon (n ~ 2.2).
RSSI_RANGE_TABLE = [
    (-30, "very close (<2 m)"),
    (-50, "close (2-10 m)"),
    (-70, "medium (10-30 m)"),
    (-85, "far (30-100 m)"),
    (-95, "edge of range (>100 m)"),
]


def rssi_to_distance_rough(rssi):
    """Return a human label for an RSSI value."""
    if rssi > -30:
        return "very close (<2 m)"
    for thresh, label in RSSI_RANGE_TABLE:
        if rssi > thresh:
            return label
    return "below threshold"


def is_locally_administered(mac):
    """A locally-administered (often randomized) address has bit 1 of byte 0 set.

    Universal OUI (company) addresses have it clear. Per the IEEE, phones and
    many devices randomize their MAC, setting this bit."""
    first = int(mac.replace(":", "").replace("-", "")[0:2], 16)
    return bool(first & 0x02)


def is_multicast(mac):
    first = int(mac.replace(":", "").replace("-", "")[0:2], 16)
    return bool(first & 0x01)


def mac_randomization_label(mac):
    """Describe a MAC's address bits for study."""
    first = int(mac.replace(":", "").replace("-", "")[0:2], 16)
    parts = []
    parts.append("multicast" if first & 0x01 else "unicast")
    parts.append("locally-administered/randomized" if first & 0x02
                 else "universal (OUI-assigned)")
    return ", ".join(parts)


def multilaterate(anchors):
    """
    Solve position from a set of (x, y, distance) measurements using a simple
    least-squares linearization of the distance equations. This is the
    educational version of RSSI-based multilateration: you supply the anchor
    coordinates (where each receiver was) and the estimated distances to the
    transmitter, and it returns a best-fit (x, y, residual).

    anchors : list of (x, y, r)  (meters)

    Returns (x, y, residual) or None if there are too few anchors.

    Method: subtract the first anchor's equation from the others to linearize,
    then solve the overdetermined system with normal equations.
    """
    if len(anchors) < 3:
        return None
    x0, y0, r0 = anchors[0]
    rows = []
    rhs = []
    for x, y, r in anchors[1:]:
        # 2*(x-x0)*X + 2*(y-y0)*Y = r0^2 - r^2 + x^2 - x0^2 + y^2 - y0^2
        rows.append([2 * (x - x0), 2 * (y - y0)])
        rhs.append(r0 * r0 - r * r + x * x - x0 * x0 + y * y - y0 * y0)
    # Normal equations: A^T A p = A^T b
    import numpy as np  # numpy is a heavy dep; fall back if absent
    A = np.array(rows, dtype=float)
    b = np.array(rhs, dtype=float)
    try:
        p, *_ = np.linalg.lstsq(A, b, rcond=None)
    except Exception:
        return None
    X, Y = float(p[0]), float(p[1])
    residual = sum((math.hypot(x - X, y - Y) - r) ** 2 for x, y, r in anchors)
    return (round(X, 2), round(Y, 2), round(residual, 3))


def multilaterate_no_numpy(anchors):
    """Pure-stdlib version of multilaterate (2x2 solve via Cramer's rule)."""
    if len(anchors) < 3:
        return None
    x0, y0, r0 = anchors[0]
    # Take the first two other anchors to form a 2x2 system (uses 3 anchors).
    av = []
    for x, y, r in anchors[1:3]:
        A = 2 * (x - x0)
        B = 2 * (y - y0)
        C = r0 * r0 - r * r + x * x - x0 * x0 + y * y - y0 * y0
        av.append((A, B, C))
    (A1, B1, C1), (A2, B2, C2) = av
    det = A1 * B2 - A2 * B1
    if abs(det) < 1e-9:
        return None
    X = (C1 * B2 - C2 * B1) / det
    Y = (A1 * C2 - A2 * C1) / det
    residual = sum((math.hypot(x - X, y - Y) - r) ** 2 for x, y, r in anchors)
    return (round(X, 2), round(Y, 2), round(residual, 3))