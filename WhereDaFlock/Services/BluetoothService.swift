import Foundation
import CoreBluetooth
import Combine

class BluetoothService: NSObject, ObservableObject {
    private var centralManager: CBCentralManager!
    
    @Published var isScanning: Bool = false
    @Published var detectedDevices: [DetectedBluetoothDevice] = []
    @Published var isBluetoothAvailable: Bool = false
    
    private let knownBeacons: [String: DetectedBluetoothDevice.BTDeviceType] = [
        "AC233FA0": .airtag,    // Apple AirTag prefix
        "4C000000": .beacon,    // iBeacon prefix
        "F0000000": .vehicle,   // Vehicle BLE
    ]
    
    override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: .main)
    }
    
    func startScanning() {
        guard isBluetoothAvailable else { return }
        isScanning = true
        // Scan for all BLE devices
        centralManager.scanForPeripherals(withServices: nil, options: [
            CBCentralManagerScanOptionAllowDuplicatesKey: true
        ])
        
        // Auto-stop after 30 seconds to save battery
        DispatchQueue.main.asyncAfter(deadline: .now() + 30) { [weak self] in
            self?.stopScanning()
        }
    }
    
    func stopScanning() {
        centralManager.stopScan()
        isScanning = false
    }
    
    func clearDevices() {
        detectedDevices.removeAll()
    }
    
    // Flock Safety BLE Company Identifier (manufacturer-data first 2 bytes,
    // little-endian). Matches firmware/src/ble_signatures.h.
    private let flockMfrID: UInt16 = 0x09C8
    
    // Returns true if the advertisement's manufacturer data begins with the
    // Flock Company ID (little-endian). Mirrors hasFlockMfrId() in the firmware.
    private func isFlockManufacturerData(_ manufacturerData: Data?) -> Bool {
        guard let data = manufacturerData, data.count >= 2 else { return false }
        let mfrID = UInt16(data[data.startIndex]) | (UInt16(data[data.startIndex + 1]) << 8)
        return mfrID == flockMfrID
    }
    
    private func classifyDevice(name: String?, uuid: UUID, manufacturerData: Data?) -> DetectedBluetoothDevice.BTDeviceType {
        // Decisive Flock signal: manufacturer Company ID 0x09C8.
        if isFlockManufacturerData(manufacturerData) { return .flock }
        
        guard let name = name?.uppercased() else { return .unknown }
        
        if name.contains("FLOCK") { return .flock }
        if name.contains("AIR") || name.contains("TAG") { return .airtag }
        if name.contains("BEACON") || name.contains("IBEACON") { return .beacon }
        if name.contains("CAR") || name.contains("BMW") || name.contains("FORD") || name.contains("TESLA") || name.contains("HONDA") || name.contains("TOYOTA") || name.contains("AUDI") || name.contains("MERCEDES") {
            return .vehicle
        }
        if name.contains("WATCH") || name.contains("FITBIT") || name.contains("GARMIN") { return .wearable }
        if name.contains("LIGHT") || name.contains("SENSOR") || name.contains("TRACK") { return .iot }
        
        for (prefix, type) in knownBeacons {
            if uuid.uuidString.uppercased().hasPrefix(prefix) { return type }
        }
        
        return .unknown
    }
    
    private func estimateDistance(rssi: Int) -> Double {
        let txPower = -59 // Reference RSSI at 1 meter
        if rssi == 0 { return -1 }
        let ratio = Double(rssi) / Double(txPower)
        if ratio < 1.0 {
            return pow(ratio, 10)
        } else {
            return (0.89976 * pow(ratio, 7.7095)) + 0.111
        }
    }
}

// MARK: - CBCentralManagerDelegate
extension BluetoothService: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn:
            isBluetoothAvailable = true
        case .poweredOff, .unauthorized, .unsupported, .resetting, .unknown:
            isBluetoothAvailable = false
            isScanning = false
        @unknown default:
            isBluetoothAvailable = false
        }
    }
    
    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String: Any], rssi RSSI: NSNumber) {
        let name = peripheral.name ?? advertisementData[CBAdvertisementDataLocalNameKey] as? String
        let mfrData = advertisementData[CBAdvertisementDataManufacturerDataKey] as? Data
        let deviceType = classifyDevice(name: name, uuid: peripheral.identifier, manufacturerData: mfrData)
        let distance = estimateDistance(rssi: RSSI.intValue)
        
        let device = DetectedBluetoothDevice(
            name: name,
            uuid: peripheral.identifier,
            rssi: RSSI.intValue,
            estimatedDistance: distance,
            deviceType: deviceType,
            firstSeen: Date(),
            lastSeen: Date()
        )
        
        DispatchQueue.main.async { [weak self] in
            guard let self = self else { return }
            if let index = self.detectedDevices.firstIndex(where: { $0.uuid == device.uuid }) {
                var existing = self.detectedDevices[index]
                existing.rssi = device.rssi
                existing.estimatedDistance = device.estimatedDistance
                existing.lastSeen = device.lastSeen
                self.detectedDevices[index] = existing
            } else {
                self.detectedDevices.append(device)
            }
        }
    }
}
