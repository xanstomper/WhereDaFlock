import SwiftUI

struct DetectionDashboardView: View {
    @EnvironmentObject var locationService: LocationService
    @StateObject private var cameraService = CameraDatabaseService()
    @StateObject private var bluetoothService = BluetoothService()
    @StateObject private var reportService = ReportService()
    
    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(spacing: 16) {
                    RiskLevelCard(cameras: nearbyCameras.count,
                                 reports: nearbyReports.count,
                                 riskLevel: calculateRiskLevel())
                    
                    GroupBox {
                        if nearbyCameras.isEmpty {
                            Text("No cameras nearby").foregroundColor(.secondary).padding()
                        } else {
                            ForEach(nearbyCameras) { camera in
                                CameraRow(camera: camera)
                            }
                        }
                    } label: {
                        Label("Nearby Cameras", systemImage: "camera.fill").foregroundColor(.orange)
                    }
                    
                    GroupBox {
                        if bluetoothService.detectedDevices.isEmpty {
                            HStack {
                                Text("Tap scan to detect devices").foregroundColor(.secondary)
                                Spacer()
                                Button(action: { bluetoothService.startScanning() }) {
                                    Image(systemName: "antenna.radiowaves.left.and.right")
                                }
                            }.padding()
                        } else {
                            ForEach(bluetoothService.detectedDevices.prefix(5)) { device in
                                BluetoothRow(device: device)
                            }
                        }
                    } label: {
                        Label("Bluetooth", systemImage: "antenna.radiowaves.left.and.right").foregroundColor(.blue)
                    }
                    
                    GroupBox {
                        if nearbyReports.isEmpty {
                            Text("No reports in this area").foregroundColor(.secondary).padding()
                        } else {
                            ForEach(nearbyReports) { report in
                                ReportRow(report: report)
                            }
                        }
                    } label: {
                        Label("Community Reports", systemImage: "exclamationmark.bubble.fill").foregroundColor(.red)
                    }
                }
                .padding()
            }
            .navigationTitle("Detection")
            .navigationBarTitleDisplayMode(.large)
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button(action: { bluetoothService.clearDevices() }) {
                        Image(systemName: "arrow.clockwise")
                    }
                }
            }
        }
        .onAppear { cameraService.refreshIfNeeded() }
    }
    
    private var nearbyCameras: [Camera] {
        guard let loc = locationService.userLocation else { return [] }
        return cameraService.camerasNear(location: loc, radiusMeters: 500)
    }
    
    private var nearbyReports: [UserReport] {
        guard let loc = locationService.userLocation else { return [] }
        return reportService.reportsNear(location: loc, radiusMeters: 500)
    }
    
    private func calculateRiskLevel() -> Double {
        let cameraRisk = min(Double(nearbyCameras.count) * 0.15, 0.6)
        let reportRisk = min(Double(nearbyReports.count) * 0.1, 0.3)
        return min(cameraRisk + reportRisk, 1.0)
    }
}
