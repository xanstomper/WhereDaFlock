import Foundation
import CoreMotion
import CoreLocation
import Combine

class SensorFusionService: ObservableObject {
    private let motionManager = CMMotionManager()
    
    @Published var currentReading: SensorReading?
    @Published var isDriving: Bool = false
    @Published var roadQuality: RoadQuality = .unknown
    @Published var movementState: MovementState = .stationary
    
    enum RoadQuality: String {
        case smooth = "Smooth"
        case rough = "Rough"
        case bumpy = "Bumpy"
        case construction = "Construction"
        case unknown = "Unknown"
    }
    
    enum MovementState: String {
        case stationary = "Stationary"
        case walking = "Walking"
        case driving = "Driving"
        case idling = "Idling"
    }
    
    private var lastAccelerations: [CMAcceleration] = []
    private let maxAccelSamples = 50
    private var speedHistory: [Double] = []
    
    func startMonitoring() {
        guard motionManager.isAccelerometerAvailable else { return }
        
        motionManager.accelerometerUpdateInterval = 0.1
        motionManager.startAccelerometerUpdates(to: .main) { [weak self] data, error in
            guard let data = data, let self = self else { return }
            
            self.lastAccelerations.append(data.acceleration)
            if self.lastAccelerations.count > self.maxAccelSamples {
                self.lastAccelerations.removeFirst()
            }
            
            self.analyzeMotion(data.acceleration)
        }
        
        if motionManager.isGyroAvailable {
            motionManager.gyroUpdateInterval = 0.1
            motionManager.startGyroUpdates(to: .main) { [weak self] data, error in
                guard let data = data else { return }
                // Process gyro data for turn detection
            }
        }
    }
    
    func stopMonitoring() {
        motionManager.stopAccelerometerUpdates()
        motionManager.stopGyroUpdates()
    }
    
    private func analyzeMotion(_ acceleration: CMAcceleration) {
        let magnitude = sqrt(acceleration.x * acceleration.x +
                             acceleration.y * acceleration.y +
                             acceleration.z * acceleration.z)
        
        // Detect driving based on vibration patterns
        if magnitude > 1.5 && magnitude < 3.0 {
            roadQuality = .rough
        } else if magnitude >= 3.0 {
            roadQuality = .bumpy
        } else if magnitude < 1.2 {
            roadQuality = .smooth
        }
        
        // Detect speed bumps
        if magnitude > 4.0 {
            roadQuality = .construction
        }
    }
    
    func updateMovementState(speed: CLLocationSpeed) {
        switch speed {
        case 0:
            movementState = .stationary
            isDriving = false
        case 0...2:
            movementState = .idling
            isDriving = false
        case 2...8:
            movementState = .walking
            isDriving = false
        default:
            movementState = .driving
            isDriving = true
        }
    }
}
