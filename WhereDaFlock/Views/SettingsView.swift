import SwiftUI

struct SettingsView: View {
    @EnvironmentObject var privacyService: PrivacyService
    @StateObject private var settings = UserSettingsStore()
    @State private var showPrivacyDashboard = false
    
    var body: some View {
        NavigationStack {
            List {
                // Privacy
                Section {
                    Button(action: { showPrivacyDashboard.toggle() }) {
                        HStack {
                            Image(systemName: "hand.raised.fill").foregroundColor(.green).font(.title2)
                            VStack(alignment: .leading) {
                                Text("Privacy Dashboard").font(.subheadline.weight(.medium))
                                Text("Score: \(Int(privacyService.privacyStatus.score))%").font(.caption).foregroundColor(.secondary)
                            }
                            Spacer()
                            Image(systemName: "chevron.right").font(.caption).foregroundColor(.secondary)
                        }
                    }
                    Toggle(isOn: $privacyService.isGhostMode) {
                        Label("Ghost Mode", systemImage: "moon.fill")
                    }.tint(.green)
                    .onChange(of: privacyService.isGhostMode) { _, newValue in
                        newValue ? privacyService.enableGhostMode() : privacyService.disableGhostMode()
                    }
                } header: { Label("Privacy", systemImage: "lock.shield.fill") }
                
                // Alerts
                Section {
                    Toggle(isOn: $settings.voiceAlertsOn) { Label("Voice Alerts", systemImage: "speaker.wave.2.fill") }
                    Toggle(isOn: $settings.hapticAlertsOn) { Label("Haptic Alerts", systemImage: "iphone.radiowaves.left.and.right") }
                    VStack(alignment: .leading) {
                        Text("Alert Distance: \(Int(settings.alertDistanceFeet)) ft").font(.subheadline)
                        Slider(value: $settings.alertDistanceFeet, in: 100...1000, step: 50)
                    }
                } header: { Label("Alerts", systemImage: "bell.fill") }
                
                // Map
                Section {
                    Toggle(isOn: $settings.showCameraLayer) { Label("Camera Layer", systemImage: "camera.fill") }
                    Toggle(isOn: $settings.showReportLayer) { Label("Report Layer", systemImage: "exclamationmark.bubble.fill") }
                    Toggle(isOn: $settings.showTrafficLayer) { Label("Traffic Layer", systemImage: "car.fill") }
                    Toggle(isOn: $settings.useMetric) { Label("Metric Units", systemImage: "ruler") }
                } header: { Label("Map", systemImage: "map.fill") }
                
                // Navigation
                Section {
                    Picker("Preferred Route", selection: $settings.preferredRouteMode) {
                        ForEach(RoutePreference.allCases, id: \.self) { mode in
                            Text(mode.rawValue).tag(mode)
                        }
                    }
                    Toggle(isOn: $settings.offlineMode) { Label("Offline Mode", systemImage: "wifi.slash") }
                } header: { Label("Navigation", systemImage: "location.north.line.fill") }
                
                // About
                Section {
                    HStack { Text("Version"); Spacer(); Text("1.0.0").foregroundColor(.secondary) }
                    HStack { Text("Data stored"); Spacer(); Text("\(privacyService.privacyStatus.dataStoredBytes) bytes").foregroundColor(.secondary) }
                } header: { Label("About", systemImage: "info.circle.fill") }
            }
            .navigationTitle("Settings")
            .sheet(isPresented: $showPrivacyDashboard) {
                PrivacyDashboardView(isPresented: $showPrivacyDashboard)
            }
        }
    }
}
