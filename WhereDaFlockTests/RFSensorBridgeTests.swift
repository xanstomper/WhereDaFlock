import XCTest
@testable import WhereDaFlock

final class RFSensorBridgeTests: XCTestCase {
    
    var bridge: RFSensorBridge!
    
    override func setUp() {
        super.setUp()
        bridge = RFSensorBridge()
        bridge.reset()
    }
    
    override func tearDown() {
        bridge.reset()
        bridge = nil
        super.tearDown()
    }
    
    func testJSONLineDecoding() {
        let sampleNDJSON = """
        {"event":"detection","detection_method":"wildcard_probe_ie_sig","detection_tier":4,"protocol":"wifi_2_4ghz","mac_address":"82:6B:F2:A1:B2:C3","rssi":-62,"channel":6,"frequency":2437,"ssid":""}
        """
        
        let data = sampleNDJSON.data(using: .utf8)!
        let decoder = JSONDecoder()
        do {
            let event = try decoder.decode(WDFDetectionEvent.self, from: data)
            XCTAssertEqual(event.event, "detection")
            XCTAssertEqual(event.detectionMethod, "wildcard_probe_ie_sig")
            XCTAssertEqual(event.detectionTier, 4)
            XCTAssertEqual(event.protocolType, "wifi_2_4ghz")
            XCTAssertEqual(event.macAddress, "82:6B:F2:A1:B2:C3")
            XCTAssertEqual(event.rssi, -62)
            XCTAssertEqual(event.channel, 6)
            XCTAssertEqual(event.frequency, 2437)
        } catch {
            XCTFail("Failed to decode valid detection JSON: \(error)")
        }
    }
    
    func testAscendingHopSequenceDetection() {
        let now = Date()
        let ascendingHistory: [(channel: Int, time: Date)] = [
            (channel: 1, time: now),
            (channel: 6, time: now.addingTimeInterval(0.15)),
            (channel: 11, time: now.addingTimeInterval(0.30))
        ]
        
        XCTAssertTrue(bridge.detectAscendingHop(history: ascendingHistory))
    }
    
    func testDescendingHopSequenceIgnored() {
        let now = Date()
        let descendingHistory: [(channel: Int, time: Date)] = [
            (channel: 11, time: now),
            (channel: 6, time: now.addingTimeInterval(0.12)),
            (channel: 1, time: now.addingTimeInterval(0.24))
        ]
        
        XCTAssertFalse(bridge.detectAscendingHop(history: descendingHistory))
    }
    
    func testSlowHopSequenceExceedingThresholdIgnored() {
        let now = Date()
        // Delay > 350ms between hops
        let slowHistory: [(channel: Int, time: Date)] = [
            (channel: 1, time: now),
            (channel: 6, time: now.addingTimeInterval(0.50))
        ]
        
        XCTAssertFalse(bridge.detectAscendingHop(history: slowHistory))
    }
    
    func testRecordEventAndBurstTrigger() {
        let mac = "70:C9:4E:11:22:33"
        let event1 = WDFDetectionEvent(
            detectionMethod: "wildcard_probe",
            detectionTier: 3,
            macAddress: mac,
            rssi: -65,
            channel: 1,
            frequency: 2412
        )
        let event2 = WDFDetectionEvent(
            detectionMethod: "wildcard_probe",
            detectionTier: 3,
            macAddress: mac,
            rssi: -64,
            channel: 6,
            frequency: 2437
        )
        
        bridge.recordEvent(event1)
        XCTAssertFalse(bridge.isBurstDetected)
        XCTAssertEqual(bridge.activeCameraCount, 1)
        
        bridge.recordEvent(event2)
        XCTAssertTrue(bridge.isBurstDetected)
        XCTAssertNotNil(bridge.lastBurstTime)
        XCTAssertEqual(bridge.activeCameraCount, 1)
    }
    
    func testActiveCameraCountingUniqueMACs() {
        let eventA = WDFDetectionEvent(detectionMethod: "oui_match", detectionTier: 2, macAddress: "82:6B:F2:00:00:01", rssi: -70, channel: 1, frequency: 2412)
        let eventB = WDFDetectionEvent(detectionMethod: "oui_match", detectionTier: 2, macAddress: "82:6B:F2:00:00:02", rssi: -72, channel: 1, frequency: 2412)
        let eventC = WDFDetectionEvent(detectionMethod: "oui_match", detectionTier: 2, macAddress: "82:6B:F2:00:00:01", rssi: -68, channel: 1, frequency: 2412)
        
        bridge.recordEvent(eventA)
        bridge.recordEvent(eventB)
        bridge.recordEvent(eventC)
        
        // 2 unique MACs among 3 recorded events
        XCTAssertEqual(bridge.activeCameraCount, 2)
        XCTAssertEqual(bridge.recentDetections.count, 3)
    }
    
    func testResetClearsState() {
        let event = WDFDetectionEvent(detectionMethod: "oui_match", detectionTier: 2, macAddress: "70:C9:4E:AA:BB:CC", rssi: -60, channel: 6, frequency: 2437)
        bridge.recordEvent(event)
        XCTAssertEqual(bridge.activeCameraCount, 1)
        
        bridge.reset()
        XCTAssertEqual(bridge.activeCameraCount, 0)
        XCTAssertEqual(bridge.recentDetections.count, 0)
        XCTAssertFalse(bridge.isBurstDetected)
        XCTAssertNil(bridge.lastBurstTime)
    }
}
