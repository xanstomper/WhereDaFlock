import XCTest
import CoreLocation
@testable import WhereDaFlock

final class RouteScorerTests: XCTestCase {
    
    func testCleanCorridorHasMaxScores() {
        let coords = [
            CLLocationCoordinate2D(latitude: 37.7749, longitude: -122.4194),
            CLLocationCoordinate2D(latitude: 37.7759, longitude: -122.4184)
        ]
        let cameras: [Camera] = []
        
        let score = RouteScorer.score(coordinates: coords, cameras: cameras)
        
        XCTAssertEqual(score.cameraCount, 0)
        XCTAssertEqual(score.reportCount, 0)
        XCTAssertEqual(score.privacyScore, 100.0)
        XCTAssertEqual(score.riskScore, 100.0)
        XCTAssertEqual(score.corridorRating, "Clean Corridor")
    }
    
    func testSingleCameraInCorridorDetectsExposure() {
        let coords = [
            CLLocationCoordinate2D(latitude: 37.7749, longitude: -122.4194),
            CLLocationCoordinate2D(latitude: 37.7755, longitude: -122.4194)
        ]
        
        let camera = Camera(
            id: "cam-close",
            type: .flock,
            coordinate: CLLocationCoordinate2D(latitude: 37.7750, longitude: -122.4194), // < 20m from route
            address: "Market St",
            owner: "Flock Safety",
            lastVerified: Date(),
            confidence: 95.0,
            isConfirmed: true,
            source: Camera.CameraSource(name: "Test", reliability: 1.0, lastUpdated: Date())
        )
        
        let score = RouteScorer.score(coordinates: coords, cameras: [camera])
        
        XCTAssertEqual(score.cameraCount, 1)
        XCTAssertLessThan(score.privacyScore, 100.0)
        XCTAssertEqual(score.corridorRating, "Surveillance Detected")
    }
    
    func testDistantCameraOutsideBufferIgnored() {
        let coords = [
            CLLocationCoordinate2D(latitude: 37.7749, longitude: -122.4194),
            CLLocationCoordinate2D(latitude: 37.7755, longitude: -122.4194)
        ]
        
        let distantCamera = Camera(
            id: "cam-distant",
            type: .flock,
            coordinate: CLLocationCoordinate2D(latitude: 37.7900, longitude: -122.4194), // > 1.5km away
            address: "Far Away",
            owner: "Flock Safety",
            lastVerified: Date(),
            confidence: 95.0,
            isConfirmed: true,
            source: Camera.CameraSource(name: "Test", reliability: 1.0, lastUpdated: Date())
        )
        
        let score = RouteScorer.score(coordinates: coords, cameras: [distantCamera])
        
        XCTAssertEqual(score.cameraCount, 0)
        XCTAssertEqual(score.corridorRating, "Clean Corridor")
    }
    
    func testHighSurveillanceDensityRating() {
        let coords = [
            CLLocationCoordinate2D(latitude: 37.7749, longitude: -122.4194),
            CLLocationCoordinate2D(latitude: 37.7750, longitude: -122.4194)
        ]
        
        var cameras: [Camera] = []
        for i in 1...6 {
            cameras.append(Camera(
                id: "cam-\(i)",
                type: .flock,
                coordinate: CLLocationCoordinate2D(latitude: 37.77491 + Double(i)*0.00001, longitude: -122.4194),
                address: "Cluster",
                owner: "Flock Safety",
                lastVerified: Date(),
                confidence: 95.0,
                isConfirmed: true,
                source: Camera.CameraSource(name: "Test", reliability: 1.0, lastUpdated: Date())
            ))
        }
        
        let score = RouteScorer.score(coordinates: coords, cameras: cameras)
        
        XCTAssertEqual(score.cameraCount, 6)
        XCTAssertEqual(score.corridorRating, "High Surveillance Density")
    }
}
