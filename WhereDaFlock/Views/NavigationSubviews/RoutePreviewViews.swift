import SwiftUI
import MapKit

struct MapViewPreview: View {
    var route: MKRoute?
    var alternateRoutes: [ScoredRoute]
    
    var body: some View {
        ZStack {
            RoundedRectangle(cornerRadius: 16)
                .fill(Color(.systemGray6))
            if route != nil {
                VStack {
                    Image(systemName: "map.fill")
                        .font(.system(size: 40))
                        .foregroundColor(.blue)
                    Text("Route Preview").font(.caption).foregroundColor(.secondary)
                }
            } else {
                VStack(spacing: 8) {
                    Image(systemName: "location.north.line.fill")
                        .font(.system(size: 40))
                        .foregroundColor(.secondary)
                    Text("Enter a destination")
                        .font(.subheadline)
                        .foregroundColor(.secondary)
                }
            }
        }
        .frame(height: 250)
    }
}

struct RouteSelectionList: View {
    let routes: [ScoredRoute]
    let onSelect: (ScoredRoute) -> Void
    
    var body: some View {
        ScrollView(.horizontal, showsIndicators: false) {
            HStack(spacing: 12) {
                ForEach(routes) { scoredRoute in
                    RouteCard(route: scoredRoute, onSelect: { onSelect(scoredRoute) })
                }
            }
        }
    }
}

struct RouteCard: View {
    let route: ScoredRoute
    let onSelect: () -> Void
    
    var body: some View {
        Button(action: onSelect) {
            VStack(alignment: .leading, spacing: 8) {
                HStack {
                    Text(route.name)
                        .font(.caption.weight(.bold))
                        .lineLimit(1)
                    Spacer()
                    Text("\(Int(route.riskScore))")
                        .font(.title3.weight(.bold))
                        .foregroundColor(route.riskScore > 80 ? .green : .orange)
                }
                HStack(spacing: 12) {
                    Label("\(String(format: "%.1f", route.distance)) mi", systemImage: "arrow.triangle.turn.up.right.circle")
                    Label("\(route.estimatedTime.minutes()) min", systemImage: "clock")
                }
                .font(.caption2)
                .foregroundColor(.secondary)
                HStack(spacing: 12) {
                    Label("\(route.cameraExposure) cameras", systemImage: "camera.fill")
                        .foregroundColor(.orange)
                    Label("\(route.reportCount) reports", systemImage: "exclamationmark.bubble.fill")
                        .foregroundColor(.red)
                }
                .font(.caption2)
            }
            .padding()
            .frame(width: 200)
            .background(.ultraThinMaterial)
            .cornerRadius(16)
        }
    }
}

extension TimeInterval {
    func minutes() -> Int { Int(self / 60) }
}
