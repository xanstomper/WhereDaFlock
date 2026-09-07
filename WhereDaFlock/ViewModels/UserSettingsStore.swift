import SwiftUI

class UserSettingsStore: ObservableObject {
    @Published var voiceAlertsOn: Bool { didSet { UserDefaults.standard.set(voiceAlertsOn, forKey: "voiceAlerts") } }
    @Published var hapticAlertsOn: Bool { didSet { UserDefaults.standard.set(hapticAlertsOn, forKey: "hapticAlerts") } }
    @Published var showCameraLayer: Bool = true { didSet { UserDefaults.standard.set(showCameraLayer, forKey: "showCameraLayer") } }
    @Published var showReportLayer: Bool = true { didSet { UserDefaults.standard.set(showReportLayer, forKey: "showReportLayer") } }
    @Published var showTrafficLayer: Bool = true { didSet { UserDefaults.standard.set(showTrafficLayer, forKey: "showTrafficLayer") } }
    @Published var alertDistanceFeet: Double = 500 { didSet { UserDefaults.standard.set(alertDistanceFeet, forKey: "alertDistance") } }
    @Published var useMetric: Bool = false { didSet { UserDefaults.standard.set(useMetric, forKey: "useMetric") } }
    @Published var preferredRouteMode: RoutePreference = .fastest { didSet { UserDefaults.standard.set(preferredRouteMode.rawValue, forKey: "preferredRoute") } }
    @Published var offlineMode: Bool = false { didSet { UserDefaults.standard.set(offlineMode, forKey: "offlineMode") } }
    @Published var batterySaverMode: Bool = false { didSet { UserDefaults.standard.set(batterySaverMode, forKey: "batterySaver") } }
    @Published var nightMode: Bool = false { didSet { UserDefaults.standard.set(nightMode, forKey: "nightMode") } }
    
    init() {
        voiceAlertsOn = UserDefaults.standard.bool(forKey: "voiceAlerts")
        hapticAlertsOn = UserDefaults.standard.bool(forKey: "hapticAlerts")
        showCameraLayer = UserDefaults.standard.object(forKey: "showCameraLayer") as? Bool ?? true
        showReportLayer = UserDefaults.standard.object(forKey: "showReportLayer") as? Bool ?? true
        showTrafficLayer = UserDefaults.standard.object(forKey: "showTrafficLayer") as? Bool ?? true
        alertDistanceFeet = UserDefaults.standard.object(forKey: "alertDistance") as? Double ?? 500
        useMetric = UserDefaults.standard.bool(forKey: "useMetric")
        offlineMode = UserDefaults.standard.bool(forKey: "offlineMode")
        batterySaverMode = UserDefaults.standard.bool(forKey: "batterySaver")
        nightMode = UserDefaults.standard.bool(forKey: "nightMode")
        let savedRoute = UserDefaults.standard.string(forKey: "preferredRoute") ?? ""
        preferredRouteMode = RoutePreference.allCases.first { $0.rawValue == savedRoute } ?? .fastest
    }
}
