import SwiftUI
import WatchKit

struct WatchContentView: View {
    @State private var nearestDistanceFt: Int = 380
    @State private var isGhostMode: Bool = true
    @State private var radarPulse: Bool = false
    @State private var isAlertActive: Bool = false
    
    var body: some View {
        ScrollView {
            VStack(spacing: 8) {
                // Radar Pulse Indicator
                ZStack {
                    Circle()
                        .stroke(isAlertActive ? Color.red.opacity(0.3) : Color.blue.opacity(0.3), lineWidth: 2)
                        .frame(width: 80, height: 80)
                        .scaleEffect(radarPulse ? 1.25 : 0.85)
                        .animation(.easeInOut(duration: 1.2).repeatForever(autoreverses: true), value: radarPulse)
                    
                    Circle()
                        .fill(isAlertActive ? Color.red.opacity(0.15) : Color.blue.opacity(0.15))
                        .frame(width: 60, height: 60)
                    
                    Image(systemName: isAlertActive ? "exclamationmark.shield.fill" : "shield.checkered")
                        .font(.system(size: 28))
                        .foregroundColor(isAlertActive ? .red : .blue)
                }
                .padding(.top, 4)
                .onAppear {
                    radarPulse = true
                }
                
                // Nearest Camera Info
                Text("\(nearestDistanceFt) FT")
                    .font(.system(size: 24, weight: .bold, design: .rounded))
                    .foregroundColor(.primary)
                
                Text("NEAREST FLOCK ALPR")
                    .font(.system(size: 9, weight: .semibold))
                    .foregroundColor(.secondary)
                
                Divider()
                    .padding(.vertical, 4)
                
                // Ghost Mode Toggle
                Toggle(isOn: $isGhostMode) {
                    HStack(spacing: 6) {
                        Image(systemName: isGhostMode ? "eye.slash.fill" : "eye.fill")
                            .font(.caption)
                            .foregroundColor(isGhostMode ? .green : .secondary)
                        Text("Ghost Mode")
                            .font(.caption)
                    }
                }
                .onChange(of: isGhostMode) { _, enabled in
                    WKInterfaceDevice.current().play(enabled ? .directionUp : .directionDown)
                }
            }
            .padding(.horizontal)
        }
    }
    
    private func triggerProximityHaptic() {
        WKInterfaceDevice.current().play(.notification)
    }
}
