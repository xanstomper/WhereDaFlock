import SwiftUI

struct DetectionDashboardView: View {
    @EnvironmentObject var locationService: LocationService
    @StateObject private var cameraService = CameraDatabaseService()
    @StateObject private var bluetoothService = BluetoothService()
    @StateObject private var reportService = ReportService()
    @ObservedObject private var rfBridge = RFSensorBridge.shared
    
    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(spacing: 16) {
                    RiskLevelCard(cameras: nearbyCameras.count,
                                 reports: nearbyReports.count,
                                 riskLevel: calculateRiskLevel())
                    
                    GroupBox {
                        if rfBridge.recentDetections.isEmpty {
                            HStack {
                                Text("No RF bursts detected").foregroundColor(.secondary)
                                Spacer()
                                Image(systemName: "waveform.path.ecg").foregroundColor(.secondary)
                            }.padding()
                        } else {
                            VStack(spacing: 8) {
                                if rfBridge.isBurstDetected {
                                    HStack {
                                        Image(systemName: "exclamationmark.triangle.fill")
                                            .foregroundColor(.red)
                                        Text("ALPR Burst: Ascending Hop 1→6→11 (< 350ms)")
                                            .font(.caption.bold())
                                            .foregroundColor(.red)
                                        Spacer()
                                    }
                                    .padding(8)
                                    .background(Color.red.opacity(0.12))
                                    .cornerRadius(8)
                                }
                                ForEach(rfBridge.recentDetections.prefix(3)) { item in
                                    HStack {
                                        VStack(alignment: .leading, spacing: 2) {
                                            Text(item.macAddress)
                                                .font(.caption.monospaced().weight(.semibold))
                                            Text("Ch \(item.channel) (\(item.frequency) MHz) • \(item.detectionMethod)")
                                                .font(.caption2)
                                                .foregroundColor(.secondary)
                                        }
                                        Spacer()
                                        Text("\(item.rssi) dBm")
                                            .font(.caption.monospaced())
                                            .foregroundColor(.secondary)
                                    }
                                    .padding(.vertical, 4)
                                }
                            }
                        }
                    } label: {
                        Label("RF Spectrum (2.4 GHz)", systemImage: "waveform.path.ecg").foregroundColor(.purple)
                    }
                    
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
        let rfRisk = rfBridge.isBurstDetected ? 0.3 : (rfBridge.activeCameraCount > 0 ? 0.15 : 0.0)
        return min(cameraRisk + reportRisk + rfRisk, 1.0)
    }
}
