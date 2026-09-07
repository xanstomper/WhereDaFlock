import SwiftUI
import CoreLocation
import Combine

// MARK: - App State
class AppState: ObservableObject {
    @Published var isOnboarded: Bool {
        didSet { UserDefaults.standard.set(isOnboarded, forKey: "isOnboarded") }
    }
    @Published var isGuestMode: Bool = true
    @Published var selectedTab: Tab = .map
    @Published var showPrivacyMode: Bool = false
    @Published var isOffline: Bool = false
    @Published var activeAlert: AlertItem?
    
    enum Tab: String, CaseIterable {
        case map = "Map"
        case navigate = "Navigate"
        case report = "Report"
        case scan = "Scan"
        case settings = "Settings"
        
        var icon: String {
            switch self {
            case .map: return "map.fill"
            case .navigate: return "location.north.line.fill"
            case .report: return "exclamationmark.bubble.fill"
            case .scan: return "camera.viewfinder"
            case .settings: return "gearshape.fill"
            }
        }
    }
    
    init() {
        self.isOnboarded = UserDefaults.standard.bool(forKey: "isOnboarded")
    }
}

struct AlertItem: Identifiable {
    let id = UUID()
    let title: String
    let message: String
    let severity: AlertSeverity
    let coordinate: CLLocationCoordinate2D?
    
    enum AlertSeverity: String {
        case info = "info"
        case warning = "warning"
        case critical = "critical"
        
        var color: Color {
            switch self {
            case .info: return .blue
            case .warning: return .orange
            case .critical: return .red
            }
        }
    }
}
