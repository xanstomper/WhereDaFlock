import Foundation
import CoreLocation
import SwiftUI

// MARK: - Report Types
enum ReportType: String, Codable, CaseIterable {
    case police = "Police Activity"
    case camera = "Camera Spotted"
    case accident = "Accident"
    case hazard = "Road Hazard"
    case construction = "Construction"
    case checkpoint = "Checkpoint"
    case speedTrap = "Speed Trap"
    case roadClosure = "Road Closure"
    case flooding = "Flooding"
    case debris = "Debris on Road"
    case animal = "Animal on Road"
    case other = "Other"
    
    var icon: String {
        switch self {
        case .police: return "shield.fill"
        case .camera: return "camera.fill"
        case .accident: return "car.2.fill"
        case .hazard: return "exclamationmark.triangle.fill"
        case .construction: return "wrench.fill"
        case .checkpoint: return "hexagon.fill"
        case .speedTrap: return "gauge.high"
        case .roadClosure: return "hand.raised.fill"
        case .flooding: return "drop.fill"
        case .debris: return "trash.fill"
        case .animal: return "pawprint.fill"
        case .other: return "questionmark.circle"
        }
    }
    
    var color: Color {
        switch self {
        case .police: return .blue
        case .camera: return .orange
        case .accident: return .red
        case .hazard: return .yellow
        case .construction: return .brown
        case .checkpoint: return .purple
        case .speedTrap: return .pink
        case .roadClosure: return .red
        case .flooding: return .cyan
        case .debris: return .gray
        case .animal: return .green
        case .other: return .secondary
        }
    }
}

// MARK: - User Report
struct UserReport: Identifiable, Codable {
    let id: String
    let type: ReportType
    let coordinate: CLLocationCoordinate2D
    let description: String?
    let timestamp: Date
    let expiryDate: Date
    let confidence: Double
    let upvotes: Int
    let downvotes: Int
    let isVerified: Bool
    let isAnonyous: Bool
}
