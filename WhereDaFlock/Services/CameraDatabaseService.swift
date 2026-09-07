import Foundation
import CoreLocation
import Combine
import MapKit

class CameraDatabaseService: ObservableObject {
    @Published var cameras: [Camera] = []
    @Published var isLoading: Bool = false
    @Published var lastUpdated: Date?
    
    private var cancellables = Set<AnyCancellable>()
    
    init() {
        loadCachedCameras()
    }
    
    func loadCameras() {
        isLoading = true
        
        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            guard let self = self else { return }
            let loaded = self.loadBundledCameras() ?? self.sampleCameras
            
            DispatchQueue.main.async {
                self.cameras = loaded
                self.lastUpdated = Date()
                self.isLoading = false
            }
        }
    }
    
    func refreshIfNeeded() {
        guard let lastUpdated else { loadCameras(); return }
        if Date().timeIntervalSince(lastUpdated) > 3600 { loadCameras() }
    }
    
    func camerasNear(location: CLLocation, radiusMeters: Double = 500) -> [Camera] {
        return cameras.filter { camera in
            let cameraLocation = CLLocation(
                latitude: camera.coordinate.latitude,
                longitude: camera.coordinate.longitude
            )
            return location.distance(from: cameraLocation) <= radiusMeters
        }
    }
    
    func nearestCamera(from location: CLLocation) -> (Camera, CLLocationDistance)? {
        let sorted = cameras.compactMap { camera -> (Camera, CLLocationDistance)? in
            let cameraLocation = CLLocation(latitude: camera.coordinate.latitude, longitude: camera.coordinate.longitude)
            let distance = location.distance(from: cameraLocation)
            return (camera, distance)
        }.sorted { $0.1 < $1.1 }
        return sorted.first
    }
    
    func cameraGroups(in region: MKCoordinateRegion, clusterRadius: CLLocationDistance = 200) -> [CameraGroup] {
        var groups: [CameraGroup] = []
        var groupedCameras = Set<String>()
        
        for camera in cameras {
            guard !groupedCameras.contains(camera.id) else { continue }
            let cameraCoord = CLLocation(latitude: camera.coordinate.latitude, longitude: camera.coordinate.longitude)
            var cluster = [camera]
            groupedCameras.insert(camera.id)
            
            for other in cameras {
                guard !groupedCameras.contains(other.id) else { continue }
                let otherCoord = CLLocation(latitude: other.coordinate.latitude, longitude: other.coordinate.longitude)
                if cameraCoord.distance(from: otherCoord) <= clusterRadius {
                    cluster.append(other)
                    groupedCameras.insert(other.id)
                }
            }
            
            let avgLat = cluster.reduce(0.0) { $0 + $1.coordinate.latitude } / Double(cluster.count)
            let avgLng = cluster.reduce(0.0) { $0 + $1.coordinate.longitude } / Double(cluster.count)
            groups.append(CameraGroup(cameras: cluster, coordinate: CLLocationCoordinate2D(latitude: avgLat, longitude: avgLng)))
        }
        return groups
    }
    
    private func cacheCameras() { /* Encrypted local cache in production */ }
    
    private func loadCachedCameras() {
        if let bundled = loadBundledCameras(), !bundled.isEmpty {
            self.cameras = bundled
            self.lastUpdated = Date()
            print("[WhereDaFlock] Pre-loaded \(bundled.count) cameras from offline bundle.")
        } else {
            self.cameras = sampleCameras
            self.lastUpdated = Date()
        }
    }
    
    private func loadBundledCameras() -> [Camera]? {
        guard let url = Bundle.main.url(forResource: "cameras", withExtension: "json"),
              let data = try? Data(contentsOf: url) else {
            return nil
        }
        let decoder = JSONDecoder()
        decoder.dateDecodingStrategy = .iso8601
        return try? decoder.decode([Camera].self, from: data)
    }
    
    // MARK: - Sample Camera Data
    private var sampleCameras: [Camera] {
        let now = Date()
        return [
            Camera(id: "cam-001", type: .flock, coordinate: CLLocationCoordinate2D(latitude: 36.728, longitude: -79.865),
                   address: "100 Main St", owner: "Flock Safety", lastVerified: now, confidence: 95, isConfirmed: true,
                   source: Camera.CameraSource(name: "DeFlock DB", reliability: 0.92, lastUpdated: now)),
            Camera(id: "cam-002", type: .traffic, coordinate: CLLocationCoordinate2D(latitude: 36.731, longitude: -79.870),
                   address: "Oak & Elm Intersection", owner: "City DOT", lastVerified: now, confidence: 98, isConfirmed: true,
                   source: Camera.CameraSource(name: "Public Records", reliability: 0.99, lastUpdated: now)),
            Camera(id: "cam-003", type: .redLight, coordinate: CLLocationCoordinate2D(latitude: 36.725, longitude: -79.860),
                   address: "Broadway & 5th", owner: "City Traffic Authority", lastVerified: now, confidence: 97, isConfirmed: true,
                   source: Camera.CameraSource(name: "Government Data", reliability: 0.98, lastUpdated: now)),
            Camera(id: "cam-004", type: .alpr, coordinate: CLLocationCoordinate2D(latitude: 36.735, longitude: -79.875),
                   address: "Highway 58 On-ramp", owner: "County Sheriff", lastVerified: now, confidence: 88, isConfirmed: false,
                   source: Camera.CameraSource(name: "User Reports", reliability: 0.75, lastUpdated: now)),
            Camera(id: "cam-005", type: .speed, coordinate: CLLocationCoordinate2D(latitude: 36.740, longitude: -79.880),
                   address: "I-581 Mile Marker 4", owner: "Virginia DOT", lastVerified: now, confidence: 99, isConfirmed: true,
                   source: Camera.CameraSource(name: "Official Database", reliability: 1.0, lastUpdated: now)),
            Camera(id: "cam-006", type: .flock, coordinate: CLLocationCoordinate2D(latitude: 36.720, longitude: -79.855),
                   address: "Community Entrance", owner: "HOA Security", lastVerified: now, confidence: 85, isConfirmed: false,
                   source: Camera.CameraSource(name: "User Reports", reliability: 0.80, lastUpdated: now)),
            Camera(id: "cam-007", type: .police, coordinate: CLLocationCoordinate2D(latitude: 36.733, longitude: -79.868),
                   address: "Police Station", owner: "City Police Dept", lastVerified: now, confidence: 90, isConfirmed: true,
                   source: Camera.CameraSource(name: "Public Records", reliability: 0.95, lastUpdated: now)),
            Camera(id: "cam-008", type: .traffic, coordinate: CLLocationCoordinate2D(latitude: 36.727, longitude: -79.862),
                   address: "Market Square", owner: "City DOT", lastVerified: now, confidence: 96, isConfirmed: true,
                   source: Camera.CameraSource(name: "Government Data", reliability: 0.99, lastUpdated: now))
        ]
    }
}
