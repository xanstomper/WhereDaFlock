import Foundation
import CoreLocation
import SwiftUI

// MARK: - Camera Types
enum CameraType: String, Codable, CaseIterable {
    case flock = "Flock Safety Camera"
    case alpr = "ALPR Camera"
    case traffic = "Traffic Camera"
    case redLight = "Red Light Camera"
    case speed = "Speed Camera"
    case police = "Police Camera"
    case unknown = "Unknown Camera"
    
    var icon: String {
        switch self {
        case .flock: return "camera.fill"
        case .alpr: return "camera.viewfinder"
        case .traffic: return "light.beacon.max.fill"
        case .redLight: return "stoplight.fill"
        case .speed: return "speedometer"
        case .police: return "shield.fill"
        case .unknown: return "questionmark.circle"
        }
    }
    
    var color: Color {
        switch self {
        case .flock: return .orange
        case .alpr: return .purple
        case .traffic: return .blue
        case .redLight: return .red
        case .speed: return .yellow
        case .police: return .blue
        case .unknown: return .gray
        }
    }
}

// MARK: - Camera Model
struct Camera: Identifiable, Codable {
    let id: String
    let type: CameraType
    let coordinate: CLLocationCoordinate2D
    let address: String?
    let owner: String?
    let lastVerified: Date
    let confidence: Double
    let isConfirmed: Bool
    let source: CameraSource
    
    struct CameraSource: Codable {
        let name: String
        let reliability: Double
        let lastUpdated: Date
    }
}

// MARK: - Camera Group
struct CameraGroup: Identifiable {
    let id = UUID()
    let cameras: [Camera]
    let coordinate: CLLocationCoordinate2D
    var count: Int { cameras.count }
}
