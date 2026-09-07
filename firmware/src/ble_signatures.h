// WhereDaFlock - BLE Multi-Threat Surveillance & Public Safety Signatures
// Covers:
//   - Flock Safety ALPR beacons (Company ID 0x09C8, FS Ext Battery, Penguin)
//   - Axon Enterprise Police Body Cams & In-Car Fleet (Axon Signal BLE 0xFE01/0xFE02, MFR 0x0283)
//   - FAA / ASTM F3411 Drone Remote ID (Service UUID 0xFFFA, RID- beacons)

#ifndef WHERE_DA_FLOCK_BLE_SIGNATURES_H
#define WHERE_DA_FLOCK_BLE_SIGNATURES_H

#include <Arduino.h>
#include "signatures.h"

namespace WhereDaFlockBLE {

// ---------------------------------------------------------------------------
// Manufacturer Company Identifiers (Bluetooth SIG assigned numbers)
// ---------------------------------------------------------------------------
static constexpr uint16_t FLOCK_MFR_ID = 0x09C8;  // Flock Safety (XUNTONG)
static constexpr uint16_t AXON_MFR_ID  = 0x0283;  // Axon Enterprise / TASER

// ---------------------------------------------------------------------------
// Advertised device-name substrings (Flock, Axon Police, Drone Remote ID)
// ---------------------------------------------------------------------------
static const char* NAME_PATTERNS[] = {
    "FLOCK",
    "FS EXT BATTERY",
    "FS-CAM",
    "FLOCK-SAFETY",
    "PENGUIN",
    "PIGVISION"
};
static constexpr size_t NAME_PATTERN_COUNT = sizeof(NAME_PATTERNS) / sizeof(NAME_PATTERNS[0]);

static const char* POLICE_NAME_PATTERNS[] = {
    "AXON",
    "SIGNAL",
    "AB3",
    "AB4",
    "FLEET3",
    "TASER"
};
static constexpr size_t POLICE_NAME_PATTERN_COUNT = sizeof(POLICE_NAME_PATTERNS) / sizeof(POLICE_NAME_PATTERNS[0]);

static const char* DRONE_NAME_PATTERNS[] = {
    "RID-",
    "DRONE",
    "DJI-",
    "SKYDIO",
    "AUTEL"
};
static constexpr size_t DRONE_NAME_PATTERN_COUNT = sizeof(DRONE_NAME_PATTERNS) / sizeof(DRONE_NAME_PATTERNS[0]);

// ---------------------------------------------------------------------------
// Service UUIDs
// ---------------------------------------------------------------------------
static const char* SERVICE_UUID_FRAGMENTS[] = {
    "180a",   // Device Information
    "180f",   // Battery Service
    "1819",   // Location & Navigation
    "3100",   // GPS Location
    "3200",   // Power / Battery
    "3300",   // Network Status
    "3400",   // Upload Stats
    "3500",   // Error Service
};
static constexpr size_t SERVICE_UUID_FRAGMENT_COUNT =
    sizeof(SERVICE_UUID_FRAGMENTS) / sizeof(SERVICE_UUID_FRAGMENTS[0]);

// Axon Signal triggers (starts bodycam recording when cruiser siren/lightbar active)
static const char* AXON_SIGNAL_UUIDS[] = {
    "fe01", "fe02", "fe57"
};
static constexpr size_t AXON_SIGNAL_UUID_COUNT = sizeof(AXON_SIGNAL_UUIDS) / sizeof(AXON_SIGNAL_UUIDS[0]);

// ASTM F3411 Drone Remote ID 16-bit Service UUID
static const char* DRONE_RID_UUID = "fffa";

// ---------------------------------------------------------------------------
// Scoring & Thresholds
// ---------------------------------------------------------------------------
static constexpr int MFR_ID_WEIGHT        = 70;
static constexpr int NAME_MATCH_WEIGHT    = 45;
static constexpr int SERVICE_UUID_WEIGHT  = 20;
static constexpr int RSSI_PROXIMITY_BONUS = 5;

static constexpr int LIKELY_THRESHOLD   = 60;
static constexpr int POSSIBLE_THRESHOLD = 30;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
inline String toUpper(String s) {
  String out;
  out.reserve(s.length());
  for (unsigned i = 0; i < s.length(); i++) {
    char c = s[i];
    out += (c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c;
  }
  return out;
}

} // namespace WhereDaFlockBLE

#endif // WHERE_DA_FLOCK_BLE_SIGNATURES_H