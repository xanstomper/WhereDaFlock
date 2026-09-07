import Foundation
import CoreLocation
import Combine

class LocationService: NSObject, ObservableObject {
    private let locationManager = CLLocationManager()
    private let geocoder = CLGeocoder()
    
    @Published var userLocation: CLLocation?
    @Published var heading: CLHeading?
    @Published var speed: CLLocationSpeed = 0
    @Published var authorizationStatus: CLAuthorizationStatus = .notDetermined
    @Published var isTracking: Bool = false
    @Published var currentAddress: String?
    
    @Published var recentLocations: [CLLocation] = []
    private let maxRecentLocations = 100
    
    override init() {
        super.init()
        locationManager.delegate = self
        locationManager.desiredAccuracy = kCLLocationAccuracyBest
        locationManager.distanceFilter = 10 // meters
        locationManager.headingFilter = 5 // degrees
        locationManager.activityType = .automotiveNavigation
        locationManager.showsBackgroundLocationIndicator = true
    }
    
    func requestAuthorization() {
        locationManager.requestAlwaysAuthorization()
    }
    
    func requestWhenInUseAuth() {
        locationManager.requestWhenInUseAuthorization()
    }
    
    func startTracking() {
        locationManager.startUpdatingLocation()
        locationManager.startUpdatingHeading()
        isTracking = true
    }
    
    func stopTracking() {
        locationManager.stopUpdatingLocation()
        locationManager.stopUpdatingHeading()
        isTracking = false
        recentLocations.removeAll()
    }
    
    func startMonitoringSignificantChanges() {
        locationManager.startMonitoringSignificantLocationChanges()
    }
    
    func distance(to location: CLLocation) -> CLLocationDistance {
        guard let userLocation else { return 0 }
        return userLocation.distance(from: location)
    }
    
    func distanceInFeet(to location: CLLocation) -> Double {
        return distance(to: location) * 3.28084
    }
    
    private func reverseGeocode(location: CLLocation) {
        geocoder.reverseGeocodeLocation(location) { [weak self] placemarks, error in
            guard let placemark = placemarks?.first else { return }
            self?.currentAddress = [
                placemark.name,
                placemark.locality,
                placemark.administrativeArea
            ].compactMap { $0 }.joined(separator: ", ")
        }
    }
}

// MARK: - CLLocationManagerDelegate
extension LocationService: CLLocationManagerDelegate {
    func locationManagerDidChangeAuthorization(_ manager: CLLocationManager) {
        authorizationStatus = manager.authorizationStatus
        
        switch manager.authorizationStatus {
        case .authorizedAlways, .authorizedWhenInUse:
            startTracking()
        case .denied, .restricted:
            isTracking = false
        case .notDetermined:
            break
        @unknown default:
            break
        }
    }
    
    func locationManager(_ manager: CLLocationManager, didUpdateLocations locations: [CLLocation]) {
        guard let location = locations.last else { return }
        userLocation = location
        speed = location.speed >= 0 ? location.speed : 0
        
        recentLocations.append(location)
        if recentLocations.count > maxRecentLocations {
            recentLocations.removeFirst(recentLocations.count - maxRecentLocations)
        }
        
        // Geocode occasionally
        if currentAddress == nil || location.timestamp.timeIntervalSinceNow < -60 {
            reverseGeocode(location: location)
        }
    }
    
    func locationManager(_ manager: CLLocationManager, didUpdateHeading newHeading: CLHeading) {
        heading = newHeading
    }
    
    func locationManager(_ manager: CLLocationManager, didFailWithError error: Error) {
        print("Location error: \(error.localizedDescription)")
    }
}
