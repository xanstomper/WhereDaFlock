import SwiftUI

struct VisionScannerView: View {
    @ObservedObject var visionService: VisionDetectionService
    @Binding var isScanning: Bool
    
    var body: some View {
        VStack(spacing: 16) {
            ZStack {
                RoundedRectangle(cornerRadius: 20)
                    .fill(Color.black)
                    .frame(height: 300)
                Image(systemName: "camera.viewfinder")
                    .font(.system(size: 60))
                    .foregroundColor(.white.opacity(0.6))
                VStack {
                    Spacer()
                    Text("Point at infrastructure")
                        .foregroundColor(.white)
                        .padding(.bottom, 20)
                }
            }
            .padding(.horizontal)
            
            if !visionService.detectedObjects.isEmpty {
                VStack(alignment: .leading, spacing: 12) {
                    Text("Detected").font(.headline)
                    ForEach(visionService.detectedObjects) { object in
                        HStack {
                            Image(systemName: "scope").foregroundColor(.blue)
                            Text(object.type.rawValue).font(.subheadline)
                            Spacer()
                            Text("\(Int(object.confidence * 100))%")
                                .font(.caption.weight(.bold))
                                .foregroundColor(object.confidence > 0.7 ? .green : .orange)
                        }
                        .padding()
                        .background(.ultraThinMaterial)
                        .cornerRadius(12)
                    }
                }
                .padding()
            }
        }
    }
}

struct BluetoothScannerView: View {
    @ObservedObject var bluetoothService: BluetoothService
    @Binding var isScanning: Bool
    
    var body: some View {
        VStack(spacing: 16) {
            HStack {
                Circle()
                    .fill(isScanning ? Color.green : Color.secondary)
                    .frame(width: 12, height: 12)
                Text(isScanning ? "Scanning" : "Tap scan to start")
                    .font(.subheadline)
                Spacer()
                Text("\(bluetoothService.detectedDevices.count) devices")
                    .font(.caption)
                    .foregroundColor(.secondary)
            }
            .padding()
            .background(.ultraThinMaterial)
            .cornerRadius(16)
            .padding(.horizontal)
            
            if bluetoothService.detectedDevices.isEmpty {
                VStack(spacing: 12) {
                    Image(systemName: "antenna.radiowaves.left.and.right")
                        .font(.system(size: 50))
                        .foregroundColor(.secondary)
                    Text("No devices found")
                        .foregroundColor(.secondary)
                }
                .padding(.top, 50)
            } else {
                ForEach(bluetoothService.detectedDevices) { device in
                    BluetoothDeviceCard(device: device)
                }
                .padding(.horizontal)
            }
        }
    }
}

struct BluetoothDeviceCard: View {
    let device: DetectedBluetoothDevice
    
    var body: some View {
        HStack(spacing: 16) {
            ZStack {
                Circle()
                    .fill(signalColor)
                    .frame(width: 44, height: 44)
                Image(systemName: "antenna.radiowaves.left.and.right")
                    .foregroundColor(.white)
            }
            VStack(alignment: .leading, spacing: 4) {
                Text(device.name ?? "Unknown Device").font(.subheadline.weight(.semibold))
                Text(device.deviceType.rawValue).font(.caption).foregroundColor(.secondary)
                HStack {
                    Text("RSSI: \(device.rssi)").font(.caption2)
                    Text("•")
                    Text("\(String(format: "%.1f", device.estimatedDistance))m").font(.caption2)
                }
                .foregroundColor(.secondary)
            }
            Spacer()
            Text(device.lastSeen.timeAgo())
                .font(.caption2)
                .foregroundColor(.secondary)
        }
        .padding()
        .background(.ultraThinMaterial)
        .cornerRadius(16)
    }
    
    private var signalColor: Color {
        switch device.rssi {
        case ..<(-80): return .red
        case ..<(-60): return .orange
        case ..<(-40): return .yellow
        default: return .green
        }
    }
}
