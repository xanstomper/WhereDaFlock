import SwiftUI
import MapKit

struct ActiveNavigationView: View {
    let route: MKRoute
    let steps: [MKRoute.Step]
    let onEnd: () -> Void
    
    @State private var currentStepIndex = 0
    
    var body: some View {
        VStack(spacing: 0) {
            // Navigation Header
            VStack(spacing: 8) {
                HStack {
                    VStack(alignment: .leading) {
                        Text(nextInstruction)
                            .font(.title2.weight(.bold))
                        Text("\(route.expectedTravelTime.minutes()) min • \(String(format: "%.1f", route.distance / 1609.34)) mi")
                            .font(.subheadline)
                            .foregroundColor(.secondary)
                    }
                    Spacer()
                    Button(action: onEnd) {
                        Text("End")
                            .fontWeight(.semibold)
                            .padding(.horizontal, 20)
                            .padding(.vertical, 10)
                            .background(Color.red)
                            .foregroundColor(.white)
                            .cornerRadius(12)
                    }
                }
                .padding()
                
                if currentStepIndex < steps.count {
                    HStack {
                        Image(systemName: "arrow.triangle.turn.up.right.circle.fill")
                            .font(.title2)
                        Text(steps[currentStepIndex].instructions)
                            .font(.callout)
                        Spacer()
                        Text("\(String(format: "%.0f", steps[currentStepIndex].distance)) m")
                            .font(.caption)
                            .foregroundColor(.secondary)
                    }
                    .padding(.horizontal)
                }
            }
            .background(.ultraThinMaterial)
            
            // Map
            MapViewRepresentable(
                region: .constant(MKCoordinateRegion(
                    center: route.polyline.coordinate,
                    span: MKCoordinateSpan(latitudeDelta: 0.01, longitudeDelta: 0.01)
                )),
                userLocation: nil,
                cameras: [],
                reports: [],
                selectedCamera: .constant(nil),
                selectedReport: .constant(nil),
                showCameraLayer: true,
                showReportLayer: true,
                showTrafficLayer: true
            )
            
            // Bottom Controls
            HStack(spacing: 30) {
                Button(action: {}) { Image(systemName: "speaker.wave.2.fill").font(.title2) }
                Button(action: {}) { Image(systemName: "plus.circle.fill").font(.title2) }
                Button(action: {}) { Image(systemName: "exclamationmark.bubble.fill").font(.title2) }
            }
            .padding()
            .background(.ultraThinMaterial)
        }
    }
    
    private var nextInstruction: String {
        guard currentStepIndex < steps.count else { return "Arriving" }
        return steps[currentStepIndex].instructions.components(separatedBy: ".").first ?? steps[currentStepIndex].instructions
    }
}
