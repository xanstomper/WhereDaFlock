import Foundation
import AudioToolbox
import AVFoundation

public class AudioCueManager {
    public static let shared = AudioCueManager()
    
    private var geigerTimer: Timer?
    private var currentInterval: TimeInterval = 1.0
    private var isPlayingGeiger: Bool = false
    
    // System Sound IDs:
    // 1104: Standard keyboard tap / subtle click
    // 1057: Tink sound
    private let clickSoundID: SystemSoundID = 1104
    private let alertSoundID: SystemSoundID = 1005
    
    private init() {}
    
    /// Starts or updates a Geiger counter acoustic cue based on distance in meters.
    /// Closer distance produces faster clicks.
    public func updateProximity(distanceMeters: Double, enabled: Bool = true) {
        guard enabled && distanceMeters >= 0 && distanceMeters <= 500 else {
            stopGeiger()
            return
        }
        
        // Calculate click cadence:
        // <= 25m:   0.08s (12.5 Hz - urgent frenzy)
        // 25..75m:  0.18s (5.5 Hz)
        // 75..150m: 0.35s (2.8 Hz)
        // 150..300m:0.65s (1.5 Hz)
        // 300..500m:1.20s (0.8 Hz)
        let targetInterval: TimeInterval
        switch distanceMeters {
        case 0...25:
            targetInterval = 0.08
        case 25...75:
            targetInterval = 0.18
        case 75...150:
            targetInterval = 0.35
        case 150...300:
            targetInterval = 0.65
        default:
            targetInterval = 1.20
        }
        
        if !isPlayingGeiger || abs(targetInterval - currentInterval) > 0.05 {
            currentInterval = targetInterval
            restartGeigerTimer()
        }
    }
    
    /// Updates Geiger cue cadence directly from BLE / WiFi RSSI (dBm).
    public func updateRSSI(rssi: Int, enabled: Bool = true) {
        guard enabled && rssi > -95 && rssi < 0 else {
            stopGeiger()
            return
        }
        
        let targetInterval: TimeInterval
        switch rssi {
        case -50...0:
            targetInterval = 0.08
        case -65...(-51):
            targetInterval = 0.18
        case -75...(-66):
            targetInterval = 0.35
        case -85...(-76):
            targetInterval = 0.65
        default:
            targetInterval = 1.20
        }
        
        if !isPlayingGeiger || abs(targetInterval - currentInterval) > 0.05 {
            currentInterval = targetInterval
            restartGeigerTimer()
        }
    }
    
    public func playSingleClick() {
        AudioServicesPlaySystemSound(clickSoundID)
    }
    
    public func playUrgentAlert() {
        AudioServicesPlayAlertSound(alertSoundID)
    }
    
    public func stopGeiger() {
        geigerTimer?.invalidate()
        geigerTimer = nil
        isPlayingGeiger = false
    }
    
    private func restartGeigerTimer() {
        geigerTimer?.invalidate()
        isPlayingGeiger = true
        
        geigerTimer = Timer.scheduledTimer(withTimeInterval: currentInterval, repeats: true) { [weak self] _ in
            self?.playSingleClick()
        }
    }
}
