import Foundation
import SwiftUI
import CoreLocation

// MARK: - Color Extensions
extension Color {
    static let background = Color("Background")
    static let surface = Color("Surface")
    static let textPrimary = Color("TextPrimary")
    static let textSecondary = Color("TextSecondary")
    static let accentGreen = Color.green
    static let accentRed = Color.red
    static let accentOrange = Color.orange
    static let accentBlue = Color.blue
    static let accentPurple = Color.purple
}

// MARK: - View Extensions
extension View {
    func cardStyle() -> some View {
        self
            .padding()
            .background(.ultraThinMaterial)
            .cornerRadius(16)
    }
    
    func glassMorphism() -> some View {
        self
            .background(.ultraThinMaterial)
            .cornerRadius(12)
    }
}

// MARK: - Location Extensions
extension CLLocationCoordinate2D: Equatable, Codable {
    public static func == (lhs: CLLocationCoordinate2D, rhs: CLLocationCoordinate2D) -> Bool {
        lhs.latitude == rhs.latitude && lhs.longitude == rhs.longitude
    }
    
    enum CodingKeys: String, CodingKey {
        case latitude
        case longitude
    }
    
    public init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        let latitude = try container.decode(CLLocationDegrees.self, forKey: .latitude)
        let longitude = try container.decode(CLLocationDegrees.self, forKey: .longitude)
        self.init(latitude: latitude, longitude: longitude)
    }
    
    public func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(latitude, forKey: .latitude)
        try container.encode(longitude, forKey: .longitude)
    }
}

// MARK: - Double Extensions
extension Double {
    func toMiles() -> Double { self * 0.000621371 }
    func toFeet() -> Double { self * 3.28084 }
    func metersToFeet() -> Double { self * 3.28084 }
    
    func formatDistance(useMetric: Bool = false) -> String {
        if useMetric {
            if self < 1000 {
                return "\(Int(self)) m"
            } else {
                return String(format: "%.1f km", self / 1000)
            }
        } else {
            let feet = self * 3.28084
            if feet < 528 {
                return "\(Int(feet)) ft"
            } else {
                return String(format: "%.1f mi", self / 1609.34)
            }
        }
    }
}

// MARK: - Date Extensions
extension Date {
    func timeAgo() -> String {
        let interval = Date().timeIntervalSince(self)
        if interval < 60 { return "Just now" }
        if interval < 3600 { return "\(Int(interval / 60))m ago" }
        if interval < 86400 { return "\(Int(interval / 3600))h ago" }
        return "\(Int(interval / 86400))d ago"
    }
}

// MARK: - Notification Names
extension Notification.Name {
    static let cameraProximityAlert = Notification.Name("cameraProximityAlert")
    static let reportSubmitted = Notification.Name("reportSubmitted")
    static let privacyModeChanged = Notification.Name("privacyModeChanged")
    static let routeUpdated = Notification.Name("routeUpdated")
}
