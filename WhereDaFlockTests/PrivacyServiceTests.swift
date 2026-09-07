import XCTest
@testable import WhereDaFlock

final class PrivacyServiceTests: XCTestCase {
    
    var privacyService: PrivacyService!
    
    override func setUp() {
        super.setUp()
        privacyService = PrivacyService()
    }
    
    override func tearDown() {
        privacyService = nil
        super.tearDown()
    }
    
    func testGhostModeToggle() {
        privacyService.disableGhostMode()
        XCTAssertFalse(privacyService.isGhostMode)
        XCTAssertFalse(privacyService.privacyStatus.isGhostMode)
        
        privacyService.enableGhostMode()
        XCTAssertTrue(privacyService.isGhostMode)
        XCTAssertTrue(privacyService.privacyStatus.isGhostMode)
        XCTAssertFalse(privacyService.privacyStatus.locationHistoryEnabled)
        XCTAssertFalse(privacyService.privacyStatus.cloudProcessingEnabled)
        XCTAssertFalse(privacyService.privacyStatus.analyticsEnabled)
    }
    
    func testPrivacyStatusScore() {
        var status = PrivacyStatus()
        // Default: history=false, cloud=false, analytics=false, ghost=false -> 100
        XCTAssertEqual(status.score, 100.0)
        
        status.locationHistoryEnabled = true // -25 -> 75
        XCTAssertEqual(status.score, 75.0)
        
        status.cloudProcessingEnabled = true // -20 -> 55
        XCTAssertEqual(status.score, 55.0)
        
        status.analyticsEnabled = true // -15 -> 40
        XCTAssertEqual(status.score, 40.0)
        
        status.isGhostMode = true // +10 -> 50
        XCTAssertEqual(status.score, 50.0)
    }
    
    func testAnonymousTokenGeneration() {
        let token1 = privacyService.generateAnonymousToken()
        let token2 = privacyService.generateAnonymousToken()
        
        XCTAssertEqual(token1.count, 32)
        XCTAssertEqual(token2.count, 32)
        XCTAssertNotEqual(token1, token2, "Tokens should be unique and randomized")
    }
    
    func testPrivacyAuditClearsTrackingKeys() {
        UserDefaults.standard.set(true, forKey: "analytics_enabled")
        UserDefaults.standard.set("test-track-123", forKey: "tracking_id")
        
        privacyService.performPrivacyAudit()
        
        XCTAssertNil(UserDefaults.standard.object(forKey: "analytics_enabled"))
        XCTAssertNil(UserDefaults.standard.object(forKey: "tracking_id"))
        XCTAssertNotNil(privacyService.privacyStatus.lastPrivacyAudit)
    }
}
