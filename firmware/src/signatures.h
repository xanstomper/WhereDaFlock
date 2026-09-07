// WhereDaFlock - 2.4GHz WiFi signatures used to identify Flock Safety hardware.
//
// Current state of the art: Flock cameras previously advertised a management
// AP (older firmware) and BLE maintenance beacons, but those channels were
// deactivated over 2025-2026 (the management AP ~Dec 2025, BLE in spring
// 2026). Today the cameras transmit wildcard 802.11 probe requests (~0.125s
// interval, ascending channels) that are identifiable PASSIVELY by:
//   1. transmitter MAC matching a known Flock OUI, and
//   2. wildcard SSID element (tag 0, length 0), optionally plus
//   3. an Information-Element fingerprint (DeFlockJoplin research).
//
// This firmware is RECEIVE-ONLY promiscuous sniffing. It never transmits,
// never associates to a network, and never sends any probe of its own.
//
// OUI list and detection tiers pulled from colonelpanichacks/flock-you
// (crediting @NitekryDPaul's nite-oui-collection and DeFlockJoplin).

#ifndef WHERE_DA_FLOCK_SIGNATURES_H
#define WHERE_DA_FLOCK_SIGNATURES_H

#include <Arduino.h>

namespace WhereDaFlock {

// ---------------------------------------------------------------------------
// TARGET OUIs - Flock Cam MAC prefixes (all lowercase, colons only).
// 31 from @NitekryDPaul (2026-07-16 revision) + 82:6b:f2 (DeFlockJoplin).
//
// Do NOT add a "skip locally-administered" filter: 82:6b:f2 has bit 1 of the
// first octet set, so that rule would silently drop a real camera.
// ---------------------------------------------------------------------------
static const char* TARGET_OUIS[] = {
  "70:c9:4e", "3c:91:80", "d8:f3:bc", "80:30:49", "b8:35:32",
  "14:5a:fc", "74:4c:a1", "08:3a:88", "9c:2f:9d", "c0:35:32",
  "94:08:53", "e4:aa:ea", "f4:6a:dd", "e0:0a:f6", "24:b2:b9",
  "00:f4:8d", "d0:39:57", "e8:d0:fc", "e0:4f:43", "b8:1e:a4",
  "70:08:94", "58:8e:81", "ec:1b:bd", "3c:71:bf", "58:00:e3",
  "90:35:ea", "5c:93:a2", "64:6e:69", "48:27:ea", "a4:cf:12",
  "14:b5:cd",
  "82:6b:f2"   // contributed by DeFlockJoplin
};
static constexpr size_t OUI_COUNT = sizeof(TARGET_OUIS) / sizeof(TARGET_OUIS[0]);

// ---------------------------------------------------------------------------
// Confidence tiers (mirror flock-you). Higher tier = more trustworthy.
// ---------------------------------------------------------------------------
enum Tier : uint8_t {
  TIER_SSID    = 0,  // SSID keyword match (off by default)
  TIER_ECHO    = 1,  // receiver-side (addr1) / BSSID (addr3) OUI - second-hand
  TIER_OUI     = 2,  // transmitter-side (addr2) OUI on any frame
  TIER_PROBE   = 3,  // OUI + wildcard SSID probe, IE fingerprint not matched
  TIER_IE_SIG  = 4,  // OUI + wildcard SSID + IE fingerprint (highest confidence)
  TIER_COUNT   = 5
};

// Detection method labels (used in JSON + filename tagging).
static const char* tierToMethodLetter(uint8_t tier) {
  switch (tier) {
    case TIER_IE_SIG: return "wildcard_probe_ie_sig";
    case TIER_PROBE:  return "wildcard_probe";
    case TIER_OUI:    return "oui_addr2";
    case TIER_ECHO:   return "oui_addr1_addr3";
    case TIER_SSID:   return "ssid";
    default:          return "unknown";
  }
}

} // namespace WhereDaFlock

#endif // WHERE_DA_FLOCK_SIGNATURES_H