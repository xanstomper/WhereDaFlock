import SwiftUI

// MARK: - Top Bar
struct TopBarView: View {
    @Binding var showSearch: Bool
    @Binding var showLayerOptions: Bool
    
    var body: some View {
        HStack(spacing: 12) {
            PrivacyIndicatorView()
            Spacer()
            IconButton(icon: "magnifyingglass") { showSearch.toggle() }
            IconButton(icon: "square.3.layers.3d") { showLayerOptions.toggle() }
        }
        .padding(.horizontal)
        .padding(.top, 8)
    }
}

struct PrivacyIndicatorView: View {
    @EnvironmentObject var privacyService: PrivacyService
    
    var body: some View {
        HStack(spacing: 6) {
            Circle()
                .fill(privacyService.isGhostMode ? Color.green : Color.secondary)
                .frame(width: 8, height: 8)
            Text(privacyService.isGhostMode ? "Ghost Mode" : "Privacy")
                .font(.caption.weight(.medium))
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 8)
        .background(.ultraThinMaterial)
        .cornerRadius(20)
    }
}

struct IconButton: View {
    let icon: String
    let action: () -> Void
    
    var body: some View {
        Button(action: action) {
            Image(systemName: icon)
                .font(.system(size: 16, weight: .semibold))
                .padding(12)
                .background(.ultraThinMaterial)
                .clipShape(Circle())
        }
    }
}
