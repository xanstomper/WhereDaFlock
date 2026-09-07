// WhereDaFlock - Multi-Threat & Surveillance Signatures
// Covers:
//   - Flock Safety ALPR (Falcon, Sparrow, Penguin)
//   - Police Cruisers & MDTs (Sierra Wireless AirLink, Cradlepoint)
//   - Police Body Cameras & Fleet In-Car Video (Axon Enterprise)
//   - Other ALPR Systems (Motorola / Vigilant Solutions, Genetec AutoVu, Verra Mobility)
//   - Commercial & Municipal CCTV Surveillance (Axis, Hanwha/Samsung, Dahua, Hikvision)
//   - Drone Remote ID (FAA / ASD-STAN Open Drone ID)
//   - Sub-GHz / Police Radio Trunking (P25 / DMR / VHF / UHF bridge)

#ifndef WHERE_DA_FLOCK_SIGNATURES_H
#define WHERE_DA_FLOCK_SIGNATURES_H

#include <Arduino.h>

namespace WhereDaFlock {

// ---------------------------------------------------------------------------
// Threat & Equipment Categories
// ---------------------------------------------------------------------------
enum TargetCategory : uint8_t {
  CAT_FLOCK_ALPR       = 0,
  CAT_POLICE_VEHICLE   = 1,
  CAT_POLICE_BODYCAM   = 2,
  CAT_OTHER_ALPR       = 3,
  CAT_SURVEILLANCE_CAM = 4,
  CAT_DRONE_UAV        = 5,
  CAT_POLICE_RADIO     = 6,
  CAT_UNKNOWN          = 7
};

static inline const char* categoryToString(TargetCategory cat) {
  switch (cat) {
    case CAT_FLOCK_ALPR:       return "FLOCK ALPR";
    case CAT_POLICE_VEHICLE:   return "POLICE MDT";
    case CAT_POLICE_BODYCAM:   return "BODYCAM";
    case CAT_OTHER_ALPR:       return "ALPR CAM";
    case CAT_SURVEILLANCE_CAM: return "CCTV CAM";
    case CAT_DRONE_UAV:        return "DRONE RID";
    case CAT_POLICE_RADIO:     return "POLICE RADIO";
    default:                   return "TARGET";
  }
}

static inline uint16_t categoryColor(TargetCategory cat) {
  switch (cat) {
    case CAT_FLOCK_ALPR:       return 0xF800; // HW_RED
    case CAT_POLICE_VEHICLE:   return 0x07FF; // TFT_CYAN
    case CAT_POLICE_BODYCAM:   return 0xF81F; // TFT_MAGENTA
    case CAT_OTHER_ALPR:       return 0xFD20; // TFT_ORANGE
    case CAT_SURVEILLANCE_CAM: return 0x07E0; // TFT_GREEN
    case CAT_DRONE_UAV:        return 0xFFE0; // TFT_YELLOW
    case CAT_POLICE_RADIO:     return 0x03FF; // Bright Blue/Cyan
    default:                   return 0xFFFF; // TFT_WHITE
  }
}

// ---------------------------------------------------------------------------
// TARGET OUIs - Flock Cam MAC prefixes (32 original OUIs preserved for compat)
// ---------------------------------------------------------------------------
static const char* TARGET_OUIS[] = {
  "70:c9:4e", "3c:91:80", "d8:f3:bc", "80:30:49", "b8:35:32",
  "14:5a:fc", "74:4c:a1", "08:3a:88", "9c:2f:9d", "c0:35:32",
  "94:08:53", "e4:aa:ea", "f4:6a:dd", "e0:0a:f6", "24:b2:b9",
  "00:f4:8d", "d0:39:57", "e8:d0:fc", "e0:4f:43", "b8:1e:a4",
  "70:08:94", "58:8e:81", "ec:1b:bd", "3c:71:bf", "58:00:e3",
  "90:35:ea", "5c:93:a2", "64:6e:69", "48:27:ea", "a4:cf:12",
  "14:b5:cd", "82:6b:f2"
};
static constexpr size_t OUI_COUNT = sizeof(TARGET_OUIS) / sizeof(TARGET_OUIS[0]);

// ---------------------------------------------------------------------------
// Comprehensive Multi-Threat OUI Database
// ---------------------------------------------------------------------------
struct OuiSignatureEntry {
  const char* oui;
  TargetCategory category;
  const char* vendor;
  const char* defaultName;
  const char* verdict;
};

static const OuiSignatureEntry ALL_TARGET_OUIS[] = {
  // --- Flock Safety ALPR Cameras ---
  {"70:c9:4e", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR", "FLOCK_CONFIRMED"},
  {"3c:91:80", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR", "FLOCK_CONFIRMED"},
  {"d8:f3:bc", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR", "FLOCK_CONFIRMED"},
  {"80:30:49", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR", "FLOCK_CONFIRMED"},
  {"b8:35:32", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR", "FLOCK_CONFIRMED"},
  {"14:5a:fc", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR", "FLOCK_CONFIRMED"},
  {"74:4c:a1", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR", "FLOCK_CONFIRMED"},
  {"08:3a:88", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR", "FLOCK_CONFIRMED"},
  {"9c:2f:9d", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR", "FLOCK_CONFIRMED"},
  {"c0:35:32", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR", "FLOCK_CONFIRMED"},
  {"94:08:53", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR", "FLOCK_CONFIRMED"},
  {"e4:aa:ea", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR", "FLOCK_CONFIRMED"},
  {"f4:6a:dd", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR", "FLOCK_CONFIRMED"},
  {"e0:0a:f6", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR", "FLOCK_CONFIRMED"},
  {"24:b2:b9", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR", "FLOCK_CONFIRMED"},
  {"00:f4:8d", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR", "FLOCK_CONFIRMED"},
  {"d0:39:57", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR", "FLOCK_CONFIRMED"},
  {"e8:d0:fc", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR", "FLOCK_CONFIRMED"},
  {"e0:4f:43", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR", "FLOCK_CONFIRMED"},
  {"b8:1e:a4", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR", "FLOCK_CONFIRMED"},
  {"70:08:94", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR", "FLOCK_CONFIRMED"},
  {"58:8e:81", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR", "FLOCK_CONFIRMED"},
  {"ec:1b:bd", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR", "FLOCK_CONFIRMED"},
  {"3c:71:bf", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR", "FLOCK_CONFIRMED"},
  {"58:00:e3", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR", "FLOCK_CONFIRMED"},
  {"90:35:ea", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR", "FLOCK_CONFIRMED"},
  {"5c:93:a2", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR", "FLOCK_CONFIRMED"},
  {"64:6e:69", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR", "FLOCK_CONFIRMED"},
  {"48:27:ea", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR", "FLOCK_CONFIRMED"},
  {"a4:cf:12", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR", "FLOCK_CONFIRMED"},
  {"14:b5:cd", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR", "FLOCK_CONFIRMED"},
  {"82:6b:f2", CAT_FLOCK_ALPR, "Flock Safety", "Flock Falcon ALPR", "FLOCK_CONFIRMED"},

  // --- Police Cruiser MDTs / In-Vehicle Routers ---
  {"00:0f:70", CAT_POLICE_VEHICLE, "Sierra Wireless", "AirLink MP70 Cruiser", "POLICE_MDT"},
  {"00:14:b7", CAT_POLICE_VEHICLE, "Sierra Wireless", "AirLink Cruiser MDT", "POLICE_MDT"},
  {"00:1e:e7", CAT_POLICE_VEHICLE, "Sierra Wireless", "AirLink MG90 Gateway", "POLICE_MDT"},
  {"00:24:d7", CAT_POLICE_VEHICLE, "Sierra Wireless", "AirLink Cruiser AP", "POLICE_MDT"},
  {"f8:7b:7a", CAT_POLICE_VEHICLE, "Sierra Wireless", "AirLink LX60 Cruiser", "POLICE_MDT"},
  {"00:30:44", CAT_POLICE_VEHICLE, "Cradlepoint Inc", "Cradlepoint IBR900", "POLICE_MDT"},
  {"70:b1:4e", CAT_POLICE_VEHICLE, "Cradlepoint Inc", "Cradlepoint IBR1700", "POLICE_MDT"},
  {"04:4a:40", CAT_POLICE_VEHICLE, "Cradlepoint Inc", "Cradlepoint Cruiser", "POLICE_MDT"},

  // --- Police Body Cameras & Fleet In-Car Video ---
  {"88:87:c0", CAT_POLICE_BODYCAM, "Axon Enterprise", "Axon Body/Fleet AP", "POLICE_BODYCAM"},
  {"00:25:df", CAT_POLICE_BODYCAM, "Axon Enterprise", "Axon Body 3/4 Video", "POLICE_BODYCAM"},

  // --- Other ALPR Systems (Motorola/Vigilant & Genetec) ---
  {"00:23:cd", CAT_OTHER_ALPR, "Vigilant / Moto", "Reaper Mobile ALPR", "ALPR_CONFIRMED"},
  {"00:0c:e5", CAT_OTHER_ALPR, "Motorola Solutions", "Recon ALPR Camera", "ALPR_CONFIRMED"},
  {"00:14:38", CAT_OTHER_ALPR, "Motorola Solutions", "Motorola ALPR Edge", "ALPR_CONFIRMED"},
  {"00:1a:e8", CAT_OTHER_ALPR, "Genetec Inc", "AutoVu SharpV ALPR", "ALPR_CONFIRMED"},
  {"00:1d:92", CAT_OTHER_ALPR, "Verra Mobility", "Red Light / Speed Cam", "TRAFFIC_CAM"},
  {"78:a5:04", CAT_OTHER_ALPR, "Verra Mobility", "Photo Enforcement", "TRAFFIC_CAM"},

  // --- Commercial & Municipal CCTV Surveillance ---
  {"00:40:8c", CAT_SURVEILLANCE_CAM, "Axis Communications", "Axis Network Camera", "CCTV_CAMERA"},
  {"ac:cc:8e", CAT_SURVEILLANCE_CAM, "Axis Communications", "Axis Surveillance Cam", "CCTV_CAMERA"},
  {"b8:a4:4f", CAT_SURVEILLANCE_CAM, "Axis Communications", "Axis Edge Camera", "CCTV_CAMERA"},
  {"00:09:18", CAT_SURVEILLANCE_CAM, "Hanwha Techwin", "Samsung/Hanwha CCTV", "CCTV_CAMERA"},
  {"00:16:6c", CAT_SURVEILLANCE_CAM, "Hanwha Techwin", "Hanwha WiseNet Cam", "CCTV_CAMERA"},
  {"3c:ef:8c", CAT_SURVEILLANCE_CAM, "Dahua Technology", "Dahua IP Camera", "CCTV_CAMERA"},
  {"44:19:b6", CAT_SURVEILLANCE_CAM, "Dahua Technology", "Dahua Surveillance", "CCTV_CAMERA"},
  {"bc:5e:cd", CAT_SURVEILLANCE_CAM, "Dahua Technology", "Dahua Traffic Cam", "CCTV_CAMERA"},
  {"40:b4:cd", CAT_SURVEILLANCE_CAM, "Hikvision", "Hikvision CCTV Cam", "CCTV_CAMERA"},
  {"e0:50:8b", CAT_SURVEILLANCE_CAM, "Hikvision", "Hikvision IP Camera", "CCTV_CAMERA"},
  {"a4:14:37", CAT_SURVEILLANCE_CAM, "Hikvision", "Hikvision Smart Cam", "CCTV_CAMERA"},

  // --- Drone Remote ID ---
  {"fa:0b:bc", CAT_DRONE_UAV, "OpenDroneID", "FAA Drone Remote ID", "DRONE_DETECTED"}
};
static constexpr size_t ALL_OUI_COUNT = sizeof(ALL_TARGET_OUIS) / sizeof(ALL_TARGET_OUIS[0]);

// ---------------------------------------------------------------------------
// SSID Keyword Signatures (Law Enforcement, Drones, ALPR APs)
// ---------------------------------------------------------------------------
struct SsidKeywordEntry {
  const char* pattern;
  TargetCategory category;
  const char* vendor;
  const char* defaultName;
  const char* verdict;
};

static const SsidKeywordEntry TARGET_SSID_KEYWORDS[] = {
  {"AXON",        CAT_POLICE_BODYCAM, "Axon Enterprise",    "Axon Body Cam AP",    "POLICE_BODYCAM"},
  {"FLEET",       CAT_POLICE_BODYCAM, "Axon Enterprise",    "Axon Fleet In-Car",    "POLICE_BODYCAM"},
  {"AIRLINK",     CAT_POLICE_VEHICLE, "Sierra Wireless",    "AirLink Cruiser AP",   "POLICE_MDT"},
  {"CRADLEPOINT", CAT_POLICE_VEHICLE, "Cradlepoint Inc",    "Cradlepoint Cruiser",  "POLICE_MDT"},
  {"POLICE",      CAT_POLICE_VEHICLE, "Law Enforcement",    "Police Mobile AP",     "POLICE_NETWORK"},
  {"SHERIFF",     CAT_POLICE_VEHICLE, "Law Enforcement",    "Sheriff Mobile AP",    "POLICE_NETWORK"},
  {"TROOPER",     CAT_POLICE_VEHICLE, "State Police",       "Trooper Cruiser AP",   "POLICE_NETWORK"},
  {"GSP",         CAT_POLICE_VEHICLE, "State Patrol",       "State Patrol Mobile",  "POLICE_NETWORK"},
  {"RID-",        CAT_DRONE_UAV,      "FAA Remote ID",      "UAV Drone Remote ID",  "DRONE_DETECTED"},
  {"REAPER",      CAT_OTHER_ALPR,     "Vigilant Solutions", "Reaper ALPR AP",       "ALPR_CONFIRMED"},
  {"AUTOVU",      CAT_OTHER_ALPR,     "Genetec Inc",        "AutoVu ALPR Network",  "ALPR_CONFIRMED"}
};
static constexpr size_t SSID_KEYWORD_COUNT = sizeof(TARGET_SSID_KEYWORDS) / sizeof(TARGET_SSID_KEYWORDS[0]);

// ---------------------------------------------------------------------------
// Confidence tiers
// ---------------------------------------------------------------------------
enum Tier : uint8_t {
  TIER_SSID    = 0,  // SSID keyword match
  TIER_ECHO    = 1,  // receiver-side (addr1) / BSSID (addr3) OUI
  TIER_OUI     = 2,  // transmitter-side (addr2) OUI on any frame
  TIER_PROBE   = 3,  // OUI + wildcard SSID probe
  TIER_IE_SIG  = 4,  // OUI + wildcard SSID + IE fingerprint
  TIER_COUNT   = 5
};

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