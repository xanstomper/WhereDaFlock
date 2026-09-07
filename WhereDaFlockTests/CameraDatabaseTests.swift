import XCTest
import CoreLocation
import MapKit
@testable import WhereDaFlock

final class CameraDatabaseTests: XCTestCase {
    
    var dbService: CameraDatabaseService!
    
    override func setUp() {
        super.setUp()
        dbService = CameraDatabaseService()
    }
    
    override func tearDown() {
        dbService = nil
        super.tearDown()
    }
    
    func testCamerasNearLocation() {
        let center = CLLocation(latitude: 36.728, longitude: -79.865)
        
        let cams500m = dbService.camerasNear(location: center, radiusMeters: 500)
        XCTAssertGreaterThan(cams500m.count, 0)
        
        // Very tight radius (1 meter)
        let cams1m = dbService.camerasNear(location: CLLocation(latitude: 0, longitude: 0), radiusMeters: 1)
        XCTAssertEqual(cams1m.count, 0)
    }
    
    func testNearestCameraCalculation() {
        let center = CLLocation(latitude: 36.728, longitude: -79.865)
        
        let nearest = dbService.nearestCamera(from: center)
        XCTAssertNotNil(nearest)
        
        if let (cam, dist) = nearest {
            XCTAssertLessThan(dist, 50.0) // Should be cam-001 at the same coordinate
            XCTAssertEqual(cam.id, "cam-001")
        }
    }
    
    func testCameraCentroidClustering() {
        let region = MKCoordinateRegion(
            center: CLLocationCoordinate2D(latitude: 36.728, longitude: -79.865),
            latitudinalMeters: 5000,
            longitudinalMeters: 5000
        )
        
        let groups = dbService.cameraGroups(in: region, clusterRadius: 200)
        XCTAssertGreaterThan(groups.count, 0)
        
        // Verify every grouped camera exists in at least one cluster
        let totalGrouped = groups.reduce(0) { $0 + $1.cameras.count }
        XCTAssertEqual(totalGrouped, dbService.cameras.count)
    }
}
