import Foundation
import SwiftUI

// MARK: - App Constants
enum Constants {
    static let appName = "WhereDaFlock"
    static let appVersion = "1.0.0"
    static let appTagline = "Navigate freely. Stay unseen."
    
    enum Map {
        static let defaultLatitude: Double = 36.728
        static let defaultLongitude: Double = -79.865
        static let defaultZoom: Double = 0.02
        static let cameraAlertDistanceFeet: Double = 500
        static let maxReportRadiusMeters: Double = 1000
        static let clusterRadiusMeters: Double = 200
    }
    
    enum Alerts {
        static let reportExpiryHours: Double = 24
        static let maxRecentAlerts: Int = 50
        static let hapticIntensity: Float = 0.7
    }
    
    enum Bluetooth {
        static let scanDuration: TimeInterval = 30
        static let maxDevices: Int = 20
    }
    
    enum Privacy {
        static let maxLocationCache: Int = 100
        static let anonymousTokenLength: Int = 32
        static let autoLockTimeout: TimeInterval = 300
    }
    
    enum API {
        static let baseURL = "https://api.wheredaflock.com"
        static let apiVersion = "v1"
        static let timeout: TimeInterval = 15
    }
}

// MARK: - App Icons
enum AppIcons {
    static let camera = "camera.fill"
    static let flockCamera = "camera.viewfinder"
    static let alert = "exclamationmark.triangle.fill"
    static let police = "shield.fill"
    static let map = "map.fill"
    static let navigation = "location.north.line.fill"
    static let report = "exclamationmark.bubble.fill"
    static let scan = "camera.viewfinder"
    static let settings = "gearshape.fill"
    static let privacy = "hand.raised.fill"
    static let ghost = "moon.fill"
    static let route = "arrow.triangle.turn.up.right.circle.fill"
    static let bluetooth = "antenna.radiowaves.left.and.right"
    static let compass = "location.north.line"
    static let speed = "speedometer"
}
