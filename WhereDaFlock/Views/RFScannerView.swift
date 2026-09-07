import SwiftUI

struct RFScannerView: View {
    @ObservedObject var rfBridge: RFSensorBridge
    @Binding var isScanning: Bool
    
    var body: some View {
        VStack(spacing: 16) {
            // Status Card
            HStack {
                Circle()
                    .fill(rfBridge.isBurstDetected ? Color.red : (isScanning ? Color.purple : Color.secondary))
                    .frame(width: 12, height: 12)
                Text(rfBridge.isBurstDetected ? "Burst Alert Active" : (isScanning ? "Monitoring 2.4 GHz Band" : "Standby"))
                    .font(.subheadline.weight(.semibold))
                Spacer()
                Text("\(rfBridge.activeCameraCount) active transmitters")
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
            .padding()
            .background(.ultraThinMaterial)
            .cornerRadius(16)
            .padding(.horizontal)
            
            // Frequency Hop Warning Banner
            if rfBridge.isBurstDetected {
                HStack(spacing: 12) {
                    Image(systemName: "bolt.horizontal.fill")
                        .font(.title2)
                        .foregroundColor(.red)
                    VStack(alignment: .leading, spacing: 2) {
                        Text("RAPID FREQUENCY HOP DETECTED")
                            .font(.caption.bold())
                            .foregroundColor(.red)
                        Text("Ascending hop sequence (1 → 6 → 11 < 350ms) matching ALPR transmitter signature.")
                            .font(.caption2)
                            .foregroundColor(.primary)
                    }
                    Spacer()
                }
                .padding()
                .background(Color.red.opacity(0.15))
                .overlay(RoundedRectangle(cornerRadius: 12).stroke(Color.red.opacity(0.4), lineWidth: 1))
                .cornerRadius(12)
                .padding(.horizontal)
            }
            
            // Channel Distribution Indicator
            VStack(alignment: .leading, spacing: 8) {
                Text("Channel Activity Distribution").font(.caption.bold()).foregroundColor(.secondary)
                HStack(spacing: 8) {
                    ChannelPill(label: "Ch 1 (2412 MHz)", count: countFor(channel: 1), color: .blue)
                    ChannelPill(label: "Ch 6 (2437 MHz)", count: countFor(channel: 6), color: .orange)
                    ChannelPill(label: "Ch 11 (2462 MHz)", count: countFor(channel: 11), color: .purple)
                }
            }
            .padding()
            .background(.ultraThinMaterial)
            .cornerRadius(16)
            .padding(.horizontal)
            
            // Event List
            if rfBridge.recentDetections.isEmpty {
                VStack(spacing: 12) {
                    Image(systemName: "waveform.path.ecg")
                        .font(.system(size: 48))
                        .foregroundColor(.secondary)
                    Text("No RF Bursts Captured")
                        .font(.subheadline)
                        .foregroundColor(.secondary)
                    Text("Connect ESP32-S3 via USB-CDC or BLE companion to stream real-time 802.11 probe bursts.")
                        .font(.caption)
                        .foregroundColor(.secondary.opacity(0.8))
                        .multilineTextAlignment(.center)
                        .padding(.horizontal, 32)
                }
                .padding(.top, 40)
            } else {
                VStack(alignment: .leading, spacing: 12) {
                    Text("Recent Transmissions (\(rfBridge.recentDetections.count))")
                        .font(.headline)
                        .padding(.horizontal)
                    
                    ForEach(rfBridge.recentDetections) { event in
                        RFEventRow(event: event)
                            .padding(.horizontal)
                    }
                }
            }
        }
    }
    
    private func countFor(channel: Int) -> Int {
        rfBridge.recentDetections.filter { $0.channel == channel }.count
    }
}

struct ChannelPill: View {
    let label: String
    let count: Int
    let color: Color
    
    var body: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(label)
                .font(.system(size: 10, weight: .bold))
                .foregroundColor(.secondary)
                .lineLimit(1)
            HStack {
                Text("\(count)")
                    .font(.title3.bold())
                    .foregroundColor(count > 0 ? color : .secondary)
                Spacer()
                Circle()
                    .fill(count > 0 ? color : Color.clear)
                    .frame(width: 8, height: 8)
            }
        }
        .padding(10)
        .frame(maxWidth: .infinity)
        .background(color.opacity(0.1))
        .cornerRadius(10)
    }
}

struct RFEventRow: View {
    let event: WDFDetectionEvent
    
    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Text(event.macAddress)
                    .font(.system(.subheadline, design: .monospaced).bold())
                Spacer()
                TierBadge(tier: event.detectionTier)
            }
            
            HStack {
                Label("Ch \(event.channel) (\(event.frequency) MHz)", systemImage: "antenna.radiowaves.left.and.right")
                    .font(.caption2)
                    .foregroundColor(.secondary)
                Spacer()
                Text("\(event.rssi) dBm")
                    .font(.system(.caption, design: .monospaced).bold())
                    .foregroundColor(rssiColor)
            }
            
            HStack {
                Text("Method: \(event.detectionMethod.replacingOccurrences(of: "_", with: " ").capitalized)")
                    .font(.caption2)
                    .foregroundColor(.secondary)
                Spacer()
                Text(event.timestamp.timeAgo())
                    .font(.caption2)
                    .foregroundColor(.secondary)
            }
        }
        .padding()
        .background(.ultraThinMaterial)
        .cornerRadius(14)
    }
    
    private var rssiColor: Color {
        switch event.rssi {
        case ..<(-80): return .red
        case ..<(-65): return .orange
        case ..<(-50): return .yellow
        default: return .green
        }
    }
}

struct TierBadge: View {
    let tier: Int
    
    var body: some View {
        Text("Tier \(tier)")
            .font(.system(size: 11, weight: .bold))
            .padding(.horizontal, 6)
            .padding(.vertical, 2)
            .background(tierColor.opacity(0.2))
            .foregroundColor(tierColor)
            .cornerRadius(6)
    }
    
    private var tierColor: Color {
        switch tier {
        case 4: return .red
        case 3: return .orange
        case 2: return .yellow
        default: return .blue
        }
    }
}
