import Foundation
import Combine
import CoreLocation

/// Represents a parsed detection line from the ESP32 scanner (USB-CDC or BLE).
public struct WDFDetectionEvent: Codable, Identifiable {
    public var id: String { "\(macAddress)-\(timestamp.timeIntervalSince1970)" }
    public let event: String
    public let detectionMethod: String
    public let detectionTier: Int
    public let protocolType: String
    public let macAddress: String
    public let rssi: Int
    public let channel: Int
    public let frequency: Int
    public let ssid: String
    public let timestamp: Date
    
    enum CodingKeys: String, CodingKey {
        case event
        case detectionMethod = "detection_method"
        case detectionTier = "detection_tier"
        case protocolType = "protocol"
        case macAddress = "mac_address"
        case rssi
        case channel
        case frequency
        case ssid
    }
    
    public init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        event = try container.decodeIfPresent(String.self, forKey: .event) ?? "detection"
        detectionMethod = try container.decodeIfPresent(String.self, forKey: .detectionMethod) ?? ""
        detectionTier = try container.decodeIfPresent(Int.self, forKey: .detectionTier) ?? 0
        protocolType = try container.decodeIfPresent(String.self, forKey: .protocolType) ?? "wifi_2_4ghz"
        macAddress = try container.decodeIfPresent(String.self, forKey: .macAddress) ?? ""
        rssi = try container.decodeIfPresent(Int.self, forKey: .rssi) ?? -90
        channel = try container.decodeIfPresent(Int.self, forKey: .channel) ?? 1
        frequency = try container.decodeIfPresent(Int.self, forKey: .frequency) ?? 2412
        ssid = try container.decodeIfPresent(String.self, forKey: .ssid) ?? ""
        timestamp = Date()
    }
    
    public init(
        event: String = "detection",
        detectionMethod: String,
        detectionTier: Int,
        protocolType: String = "wifi_2_4ghz",
        macAddress: String,
        rssi: Int,
        channel: Int,
        frequency: Int,
        ssid: String = ""
    ) {
        self.event = event
        self.detectionMethod = detectionMethod
        self.detectionTier = detectionTier
        self.protocolType = protocolType
        self.macAddress = macAddress
        self.rssi = rssi
        self.channel = channel
        self.frequency = frequency
        self.ssid = ssid
        self.timestamp = Date()
    }
}

/// Bridge service that receives raw telemetry streams and detects RF frequency hopping bursts.
public class RFSensorBridge: ObservableObject {
    public static let shared = RFSensorBridge()
    
    @Published public var recentDetections: [WDFDetectionEvent] = []
    @Published public var isBurstDetected: Bool = false
    @Published public var lastBurstTime: Date?
    @Published public var activeCameraCount: Int = 0
    
    private var cancellables = Set<AnyCancellable>()
    private let maxHistory = 50
    
    // Tracks channel transitions per MAC to detect rapid 1 -> 6 -> 11 sequence bursts (< 350ms)
    private var macChannelHistory: [String: [(channel: Int, time: Date)]] = [:]
    
    public init() {
        // Listen to companion BLE alerts
        NotificationCenter.default.publisher(for: .cameraProximityAlert)
            .compactMap { $0.object as? String }
            .sink { [weak self] rawJson in
                self?.ingestJSONLine(rawJson)
            }
            .store(in: &cancellables)
    }
    
    /// Parses an incoming NDJSON line from the ESP32 scanner over USB or BLE.
    public func ingestJSONLine(_ line: String) {
        guard let data = line.data(using: .utf8) else { return }
        let decoder = JSONDecoder()
        guard let event = try? decoder.decode(WDFDetectionEvent.self, from: data) else { return }
        
        DispatchQueue.main.async { [weak self] in
            self?.recordEvent(event)
        }
    }
    
    public func recordEvent(_ event: WDFDetectionEvent) {
        recentDetections.insert(event, at: 0)
        if recentDetections.count > maxHistory {
            recentDetections.removeLast()
        }
        
        // Update active camera count
        let uniqueMacs = Set(recentDetections.map { $0.macAddress })
        activeCameraCount = uniqueMacs.count
        
        // Analyze for RF sequence burst (ascending frequency hop)
        let now = Date()
        var history = macChannelHistory[event.macAddress] ?? []
        history.append((channel: event.channel, time: now))
        // Keep only recent 2 seconds
        history = history.filter { now.timeIntervalSince($0.time) < 2.0 }
        macChannelHistory[event.macAddress] = history
        
        if detectAscendingHop(history: history) {
            isBurstDetected = true
            lastBurstTime = now
            HapticManager.shared.cameraProximityAlert()
            AudioCueManager.shared.updateRSSI(rssi: event.rssi, enabled: true)
        }
    }
    
    public func detectAscendingHop(history: [(channel: Int, time: Date)]) -> Bool {
        guard history.count >= 2 else { return false }
        for i in 0..<(history.count - 1) {
            let ch1 = history[i].channel
            let ch2 = history[i+1].channel
            let dt = history[i+1].time.timeIntervalSince(history[i].time)
            // Cameras hop 1 -> 6 -> 11 ascending within 125ms–250ms
            if ch2 > ch1 && dt <= 0.35 {
                return true
            }
        }
        return false
    }
    
    public func reset() {
        recentDetections.removeAll()
        macChannelHistory.removeAll()
        isBurstDetected = false
        lastBurstTime = nil
        activeCameraCount = 0
    }
}
