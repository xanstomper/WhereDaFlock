import Foundation
import CoreLocation
import SwiftUI
import CoreMotion

// MARK: - Detected Bluetooth Device
struct DetectedBluetoothDevice: Identifiable {
    let id = UUID()
    let name: String?
    let uuid: UUID
    let rssi: Int
    let estimatedDistance: Double
    let deviceType: BTDeviceType
    let firstSeen: Date
    let lastSeen: Date
    
    enum BTDeviceType: String {
        case unknown = "Unknown Device"
        case airtag = "AirTag"
        case beacon = "Beacon"
        case vehicle = "Vehicle"
        case wearable = "Wearable"
        case iot = "IoT Device"
        case flock = "Flock Camera"
    }
}

// MARK: - Detected Object (Vision)
struct DetectedObject: Identifiable {
    let id = UUID()
    let type: DetectedObjectType
    let confidence: Double
    let boundingBox: CGRect
    let distanceEstimate: Double?
    
    enum DetectedObjectType: String {
        case camera = "Camera"
        case alprCamera = "ALPR Camera"
        case trafficCamera = "Traffic Camera"
        case policeVehicle = "Police Vehicle"
        case speedCamera = "Speed Camera"
        case roadSign = "Road Sign"
        case vehicle = "Vehicle"
        case pedestrian = "Pedestrian"
    }
}

// MARK: - Sensor Reading
struct SensorReading {
    let timestamp: Date
    let acceleration: CMAcceleration?
    let gyroscope: CMRotationRate?
    let magneticField: CMMagneticField?
    let heading: CLHeading?
    let gpsSpeed: CLLocationSpeed?
    let gpsAccuracy: CLLocationAccuracy?
}

// MARK: - Nearby Systems Summary
struct NearbySystemsSummary {
    var flockCameras: Int = 0
    var trafficCameras: Int = 0
    var bluetoothDevices: Int = 0
    var activeReports: Int = 0
    var riskLevel: Double = 0.0
    var nearestCameraDistance: Double?
    var nearestCameraType: CameraType?
}
