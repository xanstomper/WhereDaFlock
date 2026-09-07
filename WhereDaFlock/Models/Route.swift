import Foundation
import MapKit
import SwiftUI

// MARK: - Route Preferences
enum RoutePreference: String, CaseIterable {
    case fastest = "Fastest"
    case privacy = "Privacy Optimized"
    case calm = "Calm Route"
    case scenic = "Scenic"
    case accessible = "Accessible"
    
    var icon: String {
        switch self {
        case .fastest: return "bolt.fill"
        case .privacy: return "eye.slash.fill"
        case .calm: return "leaf.fill"
        case .scenic: return "mountain.2.fill"
        case .accessible: return "figure.roll"
        }
    }
    
    var description: String {
        switch self {
        case .fastest: return "Optimize for speed"
        case .privacy: return "Minimize data exposure"
        case .calm: return "Avoid stressful roads"
        case .scenic: return "Interesting routes"
        case .accessible: return "Easier roads & turns"
        }
    }
}

struct ScoredRoute: Identifiable {
    let id = UUID()
    let name: String
    let route: MKRoute
    let distance: Double
    let estimatedTime: TimeInterval
    let cameraExposure: Int
    let reportCount: Int
    let riskScore: Double
    let trafficLevel: TrafficLevel
    let privacyScore: Double
    
    enum TrafficLevel: String {
        case light = "Light"
        case moderate = "Moderate"
        case heavy = "Heavy"
        case severe = "Severe"
        
        var color: Color {
            switch self {
            case .light: return .green
            case .moderate: return .yellow
            case .heavy: return .orange
            case .severe: return .red
            }
        }
    }
}
