// WhereDaFlock - BLE signatures used to identify Flock Safety hardware.
//
// Flock Safety cameras broadcast Bluetooth Low Energy advertisement packets
// that carry the Flock manufacturer ID 0x09C8 (the Company Identifier is
// assigned to XUNTONG). This was discovered/verified by @wgreenberg and is the
// basis of the flock-hunter-cyd-ble detector. Additional supporting signals are
// the advertised device name and (for companion hardware) known service UUIDs.
//
// This firmware is RECEIVE-ONLY: it passively scans for advertisements and
// never advertises, connects, or writes to any device.

#ifndef WHERE_DA_FLOCK_BLE_SIGNATURES_H
#define WHERE_DA_FLOCK_BLE_SIGNATURES_H

#include <Arduino.h>

namespace WhereDaFlockBLE {

// ---------------------------------------------------------------------------
// Flock Safety BLE manufacturer Company Identifier.
//
// In a BLE advertisement, the manufacturer-specific data begins with a 2-byte
// Company Identifier stored little-endian. Flock hardware uses 0x09C8.
// JSON etc. print it as the raw value.
// ---------------------------------------------------------------------------
static constexpr uint16_t FLOCK_MFR_ID = 0x09C8;

// ---------------------------------------------------------------------------
// Advertised device-name substrings (secondary / supporting signal).
// These come from the flock-you BLE protocol documentation. A device whose
// advertised name contains any of these is worth reporting.
// ---------------------------------------------------------------------------
static const char* NAME_PATTERNS[] = {
    "FLOCK",              // generic Flock Safety
    "FS EXT BATTERY",     // Flock extended battery pack
    "FS-CAM",             // Flock camera
    "FLOCK-SAFETY",       // long form
    "PENGUIN",            // Flock trailer / mobile unit
    "PIGVISION"           // Pigvision systems
};
static constexpr size_t NAME_PATTERN_COUNT = sizeof(NAME_PATTERNS) / sizeof(NAME_PATTERNS[0]);

// ---------------------------------------------------------------------------
// Service UUIDs associated with Flock / companion maintenance services.
// These are supporting signals only (shared with generic IoT hardware), so
// they contribute to confidence but never alone flag a device.
// ---------------------------------------------------------------------------
static const char* SERVICE_UUID_FRAGMENTS[] = {
    "180a",   // Device Information
    "180f",   // Battery Service
    "1819",   // Location & Navigation (sensor rigs)
    "3100",   // GPS Location (observation / companion service base)
    "3200",   // Power / Battery (companion)
    "3300",   // Network Status (companion)
    "3400",   // Upload Stats (companion)
    "3500",   // Error Service (companion)
};
static constexpr size_t SERVICE_UUID_FRAGMENT_COUNT =
    sizeof(SERVICE_UUID_FRAGMENTS) / sizeof(SERVICE_UUID_FRAGMENTS[0]);

// ---------------------------------------------------------------------------
// Confidence scoring (0-100). MFR ID is the decisive signal.
// ---------------------------------------------------------------------------
static constexpr int MFR_ID_WEIGHT        = 70;  // confirmed Flock Company ID
static constexpr int NAME_MATCH_WEIGHT    = 45;  // strong name substring
static constexpr int SERVICE_UUID_WEIGHT  = 20;  // weak supporting signal
static constexpr int RSSI_PROXIMITY_BONUS = 5;   // bonus when very close

// Detection thresholds
static constexpr int LIKELY_THRESHOLD   = 60;   // >= : report as Flock
static constexpr int POSSIBLE_THRESHOLD = 30;   // >= : report as candidate

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