import SwiftUI
import CoreLocation

// MARK: - Bottom Overlay
struct BottomOverlayView: View {
    @ObservedObject var cameraService: CameraDatabaseService
    @ObservedObject var reportService: ReportService
    @ObservedObject var viewModel: MapViewModel
    @EnvironmentObject var locationService: LocationService
    @EnvironmentObject var appState: AppState
    
    var body: some View {
        VStack(spacing: 12) {
            NearbySummaryCard(
                cameras: cameraService.camerasNear(
                    location: locationService.userLocation ?? defaultLocation
                ),
                reports: reportService.reportsNear(
                    location: locationService.userLocation ?? defaultLocation
                )
            )
            
            HStack(spacing: 16) {
                QuickActionButton(title: "Navigate", icon: "location.north.line.fill", color: .blue) {
                    appState.selectedTab = .navigate
                }
                QuickActionButton(title: "Report", icon: "exclamationmark.bubble.fill", color: .orange) {
                    appState.selectedTab = .report
                }
                QuickActionButton(title: "Scan", icon: "camera.viewfinder", color: .purple) {
                    appState.selectedTab = .scan
                }
            }
        }
        .padding(.horizontal)
    }
    
    private var defaultLocation: CLLocation {
        CLLocation(latitude: 36.728, longitude: -79.865)
    }
}

struct NearbySummaryCard: View {
    var cameras: [Camera]
    var reports: [UserReport]
    
    var body: some View {
        HStack(spacing: 20) {
            StatItem(icon: "camera.fill", value: "\(cameras.count)", label: "Cameras", color: .orange)
            StatItem(icon: "exclamationmark.bubble.fill", value: "\(reports.count)", label: "Alerts", color: .red)
            StatItem(icon: "antenna.radiowaves.left.and.right", value: "\(Int.random(in: 1...5))", label: "BLE", color: .blue)
            StatItem(icon: "shield.fill", value: "\(Int.random(in: 40...95))", label: "Safety", color: .green)
        }
        .padding()
        .background(.ultraThinMaterial)
        .cornerRadius(16)
    }
}

struct StatItem: View {
    let icon: String
    let value: String
    let label: String
    let color: Color
    
    var body: some View {
        VStack(spacing: 4) {
            Image(systemName: icon)
                .font(.system(size: 18))
                .foregroundColor(color)
            Text(value)
                .font(.system(size: 18, weight: .bold))
            Text(label)
                .font(.caption2)
                .foregroundColor(.secondary)
        }
        .frame(maxWidth: .infinity)
    }
}

struct QuickActionButton: View {
    let title: String
    let icon: String
    let color: Color
    let action: () -> Void
    
    var body: some View {
        Button(action: action) {
            VStack(spacing: 8) {
                Image(systemName: icon)
                    .font(.system(size: 24))
                Text(title)
                    .font(.caption.weight(.medium))
            }
            .foregroundColor(.white)
            .frame(maxWidth: .infinity)
            .padding(.vertical, 16)
            .background(color.gradient)
            .cornerRadius(16)
        }
    }
}

struct LayerOptionsView: View {
    @Binding var showCameraLayer: Bool
    @Binding var showReportLayer: Bool
    @Binding var showTrafficLayer: Bool
    let onDismiss: () -> Void
    
    var body: some View {
        VStack {
            HStack {
                Spacer()
                VStack(alignment: .leading, spacing: 16) {
                    Text("Map Layers").font(.headline)
                    Toggle(isOn: $showCameraLayer) { Label("Cameras", systemImage: "camera.fill") }.tint(.orange)
                    Toggle(isOn: $showReportLayer) { Label("Reports", systemImage: "exclamationmark.bubble.fill") }.tint(.red)
                    Toggle(isOn: $showTrafficLayer) { Label("Traffic", systemImage: "car.fill") }.tint(.blue)
                }
                .padding()
                .background(.regularMaterial)
                .cornerRadius(16)
                .padding(.trailing)
            }
            Spacer()
        }
        .padding(.top, 60)
    }
}
