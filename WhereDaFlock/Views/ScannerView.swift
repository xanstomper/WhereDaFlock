import SwiftUI
import AVFoundation

struct ScannerView: View {
    @EnvironmentObject var locationService: LocationService
    @StateObject private var visionService = VisionDetectionService()
    @StateObject private var bluetoothService = BluetoothService()
    @State private var selectedMode: ScanMode = .vision
    @State private var isScanning: Bool = false
    
    enum ScanMode: String, CaseIterable {
        case vision = "Vision"
        case bluetooth = "Bluetooth"
        case environment = "Environment"
        
        var icon: String {
            switch self {
            case .vision: return "camera.viewfinder"
            case .bluetooth: return "antenna.radiowaves.left.and.right"
            case .environment: return "sensor.fill"
            }
        }
    }
    
    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                Picker("Scan Mode", selection: $selectedMode) {
                    ForEach(ScanMode.allCases, id: \.self) { mode in
                        Label(mode.rawValue, systemImage: mode.icon).tag(mode)
                    }
                }
                .pickerStyle(.segmented)
                .padding()
                
                ScrollView {
                    switch selectedMode {
                    case .vision:
                        VisionScannerView(visionService: visionService, isScanning: $isScanning)
                    case .bluetooth:
                        BluetoothScannerView(bluetoothService: bluetoothService, isScanning: $isScanning)
                    case .environment:
                        EnvironmentScannerView(locationService: locationService)
                    }
                }
            }
            .navigationTitle("Scan")
            .navigationBarTitleDisplayMode(.large)
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button(action: toggleScan) {
                        Image(systemName: isScanning ? "stop.circle.fill" : "play.circle.fill")
                            .font(.title2)
                            .foregroundColor(isScanning ? .red : .green)
                    }
                }
            }
        }
    }
    
    private func toggleScan() {
        isScanning.toggle()
        if isScanning {
            switch selectedMode {
            case .vision: break
            case .bluetooth: bluetoothService.startScanning()
            case .environment: break
            }
        } else {
            bluetoothService.stopScanning()
        }
    }
}
