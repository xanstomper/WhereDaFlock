import SwiftUI

struct SearchAndPreferencesView: View {
    @Binding var destinationText: String
    @Binding var selectedPreference: RoutePreference
    
    var body: some View {
        VStack(spacing: 12) {
            HStack {
                Image(systemName: "magnifyingglass")
                    .foregroundColor(.secondary)
                TextField("Enter destination...", text: $destinationText)
                    .textFieldStyle(.plain)
                if !destinationText.isEmpty {
                    Button(action: { destinationText = "" }) {
                        Image(systemName: "xmark.circle.fill")
                            .foregroundColor(.secondary)
                    }
                }
            }
            .padding()
            .background(.ultraThinMaterial)
            .cornerRadius(16)
            
            ScrollView(.horizontal, showsIndicators: false) {
                HStack(spacing: 10) {
                    ForEach(RoutePreference.allCases, id: \.self) { pref in
                        PreferenceChip(
                            preference: pref,
                            isSelected: selectedPreference == pref,
                            action: { selectedPreference = pref }
                        )
                    }
                }
            }
        }
        .padding()
    }
}

struct PreferenceChip: View {
    let preference: RoutePreference
    let isSelected: Bool
    let action: () -> Void
    
    var body: some View {
        Button(action: action) {
            HStack(spacing: 6) {
                Image(systemName: preference.icon)
                    .font(.caption)
                Text(preference.rawValue)
                    .font(.caption.weight(.medium))
            }
            .padding(.horizontal, 14)
            .padding(.vertical, 8)
            .background(isSelected ? Color.blue : .ultraThinMaterial)
            .foregroundColor(isSelected ? .white : .primary)
            .cornerRadius(20)
        }
    }
}
