import XCTest
import CoreBluetooth
@testable import WhereDaFlock

final class BluetoothServiceTests: XCTestCase {
    
    var bluetoothService: BluetoothService!
    
    override func setUp() {
        super.setUp()
        bluetoothService = BluetoothService()
    }
    
    override func tearDown() {
        bluetoothService = nil
        super.tearDown()
    }
    
    func testFlockManufacturerDataValidation() {
        // Flock Company ID is 0x09C8. In little-endian wire format: [0xC8, 0x09]
        let flockData = Data([0xC8, 0x09, 0x01, 0x02])
        XCTAssertTrue(bluetoothService.isFlockManufacturerData(flockData))
        
        // Apple iBeacon Company ID 0x004C: [0x4C, 0x00]
        let appleData = Data([0x4C, 0x00, 0x02, 0x15])
        XCTAssertFalse(bluetoothService.isFlockManufacturerData(appleData))
        
        // Truncated / empty data
        let shortData = Data([0xC8])
        XCTAssertFalse(bluetoothService.isFlockManufacturerData(shortData))
        XCTAssertFalse(bluetoothService.isFlockManufacturerData(Data()))
        XCTAssertFalse(bluetoothService.isFlockManufacturerData(nil))
    }
    
    func testClassifyDeviceWithFlockMfrData() {
        let flockData = Data([0xC8, 0x09, 0x00])
        let deviceType = bluetoothService.classifyDevice(name: nil, uuid: UUID(), manufacturerData: flockData)
        XCTAssertEqual(deviceType, .flock)
    }
    
    func testClassifyDeviceByName() {
        let flockType = bluetoothService.classifyDevice(name: "FLOCK-FALCON-CAM", uuid: UUID(), manufacturerData: nil)
        XCTAssertEqual(flockType, .flock)
        
        let airtagType = bluetoothService.classifyDevice(name: "Keys AirTag", uuid: UUID(), manufacturerData: nil)
        XCTAssertEqual(airtagType, .airtag)
        
        let carType = bluetoothService.classifyDevice(name: "Tesla BLE Key", uuid: UUID(), manufacturerData: nil)
        XCTAssertEqual(carType, .vehicle)
        
        let unknownType = bluetoothService.classifyDevice(name: "Random_Sensor_123", uuid: UUID(), manufacturerData: nil)
        XCTAssertEqual(unknownType, .unknown)
    }
    
    func testEstimateDistance() {
        // RSSI = 0 indicates invalid/unknown
        XCTAssertEqual(bluetoothService.estimateDistance(rssi: 0), -1.0)
        
        // RSSI = -59 is calibrated 1 meter reference
        let d1m = bluetoothService.estimateDistance(rssi: -59)
        XCTAssertEqual(d1m, 1.0, accuracy: 0.05)
        
        // Stronger RSSI (-45 dBm) should be < 1.0m
        let dClose = bluetoothService.estimateDistance(rssi: -45)
        XCTAssertLessThan(dClose, 1.0)
        XCTAssertGreaterThan(dClose, 0.0)
        
        // Weaker RSSI (-75 dBm) should be > 1.0m
        let dFar = bluetoothService.estimateDistance(rssi: -75)
        XCTAssertGreaterThan(dFar, 1.0)
    }
}
