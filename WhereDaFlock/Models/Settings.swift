import Foundation
import CoreLocation
import SwiftUI

// MARK: - Privacy Status
struct PrivacyStatus {
    var locationHistoryEnabled: Bool = false
    var cloudProcessingEnabled: Bool = false
    var analyticsEnabled: Bool = false
    var dataStoredBytes: Int = 0
    var isGhostMode: Bool = false
    var lastPrivacyAudit: Date?
    var encryptionLevel: EncryptionLevel = .standard
    
    enum EncryptionLevel: String {
        case standard = "Standard"
        case enhanced = "Enhanced"
        case maximum = "Maximum"
    }
    
    var score: Double {
        var s = 100.0
        if locationHistoryEnabled { s -= 25 }
        if cloudProcessingEnabled { s -= 20 }
        if analyticsEnabled { s -= 15 }
        if isGhostMode { s += 10 }
        return max(0, min(100, s))
    }
}

// MARK: - User Settings
struct UserSettings: Codable {
    var voiceAlertsOn: Bool = true
    var hapticAlertsOn: Bool = true
    var showCameraLayer: Bool = true
    var showReportLayer: Bool = true
    var showTrafficLayer: Bool = true
    var alertDistanceFeet: Double = 500
    var useMetric: Bool = false
    var preferredRouteMode: RoutePreference = .fastest
    var offlineMode: Bool = false
    var batterySaverMode: Bool = false
    var nightMode: Bool = false
    var reduceMotion: Bool = false
}
