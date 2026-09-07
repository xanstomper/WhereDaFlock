import SwiftUI
import CoreLocation

struct EnvironmentScannerView: View {
    @ObservedObject var locationService: LocationService
    @StateObject private var sensorService = SensorFusionService()
    
    var body: some View {
        VStack(spacing: 16) {
            GroupBox("Sensors") {
                VStack(spacing: 12) {
                    SensorRow(name: "GPS", value: locationService.isTracking ? "Active" : "Inactive", icon: "location.fill", color: locationService.isTracking ? .green : .gray)
                    SensorRow(name: "Speed", value: "\(String(format: "%.1f", locationService.speed * 2.237)) mph", icon: "speedometer", color: .blue)
                    SensorRow(name: "Heading", value: "\(Int(locationService.heading?.trueHeading ?? 0))°", icon: "location.north.line", color: .orange)
                    SensorRow(name: "Movement", value: sensorService.movementState.rawValue, icon: "figure.walk", color: .purple)
                    SensorRow(name: "Road Quality", value: sensorService.roadQuality.rawValue, icon: "road.lanes", color: roadQualityColor)
                }
            }
            .padding(.horizontal)
            
            GroupBox("Environment") {
                Text("All systems reporting normally")
                    .font(.subheadline)
                    .foregroundColor(.secondary)
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 8)
            }
            .padding(.horizontal)
        }
        .onAppear { sensorService.startMonitoring() }
        .onDisappear { sensorService.stopMonitoring() }
    }
    
    private var roadQualityColor: Color {
        switch sensorService.roadQuality {
        case .smooth: return .green
        case .rough: return .orange
        case .bumpy: return .red
        case .construction: return .yellow
        case .unknown: return .gray
        }
    }
}

struct SensorRow: View {
    let name: String
    let value: String
    let icon: String
    let color: Color
    
    var body: some View {
        HStack {
            Image(systemName: icon)
                .foregroundColor(color)
                .frame(width: 24)
            Text(name).font(.subheadline)
            Spacer()
            Text(value).font(.subheadline.weight(.medium))
        }
    }
}
