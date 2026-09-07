import SwiftUI
import CoreLocation

struct ReportView: View {
    @EnvironmentObject var locationService: LocationService
    @StateObject private var reportService = ReportService()
    @State private var selectedType: ReportType = .police
    @State private var description: String = ""
    @State private var isSubmitted: Bool = false
    @State private var showConfirmation: Bool = false
    
    let columns = Array(repeating: GridItem(.flexible(), spacing: 12), count: 3)
    
    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(spacing: 20) {
                    // Report Type Grid
                    VStack(alignment: .leading, spacing: 12) {
                        Text("What did you see?")
                            .font(.title2.weight(.bold))
                        
                        LazyVGrid(columns: columns, spacing: 12) {
                            ForEach(ReportType.allCases, id: \.self) { type in
                                ReportTypeCard(
                                    type: type,
                                    isSelected: selectedType == type,
                                    action: { selectedType = type }
                                )
                            }
                        }
                    }
                    
                    // Description
                    VStack(alignment: .leading, spacing: 8) {
                        Text("Details (optional)")
                            .font(.headline)
                        TextEditor(text: $description)
                            .frame(height: 100)
                            .padding(8)
                            .background(.ultraThinMaterial)
                            .cornerRadius(12)
                    }
                    
                    // Location Preview
                    if let location = locationService.userLocation {
                        HStack {
                            Image(systemName: "location.fill")
                                .foregroundColor(.blue)
                            Text("Current location")
                                .font(.subheadline)
                            Spacer()
                            Text("\(location.coordinate.latitude.normalized())°, \(location.coordinate.longitude.normalized())°")
                                .font(.caption)
                                .foregroundColor(.secondary)
                        }
                        .padding()
                        .background(.ultraThinMaterial)
                        .cornerRadius(12)
                    }
                    
                    // Submit Button
                    Button(action: submitReport) {
                        HStack {
                            Image(systemName: "exclamationmark.bubble.fill")
                            Text("Submit Report")
                                .fontWeight(.semibold)
                        }
                        .frame(maxWidth: .infinity)
                        .padding()
                        .background(selectedType.color.gradient)
                        .foregroundColor(.white)
                        .cornerRadius(16)
                    }
                    .disabled(isSubmitted)
                }
                .padding()
            }
            .navigationTitle("Report")
            .navigationBarTitleDisplayMode(.large)
            .alert("Report Submitted", isPresented: $showConfirmation) {
                Button("OK") {
                    resetForm()
                }
            } message: {
                Text("Your anonymous report helps the community.")
            }
        }
    }
    
    private func submitReport() {
        guard let location = locationService.userLocation else { return }
        reportService.submitReport(
            type: selectedType,
            coordinate: location.coordinate,
            description: description.isEmpty ? nil : description
        )
        isSubmitted = true
        showConfirmation = true
        HapticManager.shared.reportSubmitted()
    }
    
    private func resetForm() {
        selectedType = .police
        description = ""
        isSubmitted = false
    }
}

struct ReportTypeCard: View {
    let type: ReportType
    let isSelected: Bool
    let action: () -> Void
    
    var body: some View {
        Button(action: action) {
            VStack(spacing: 8) {
                Image(systemName: type.icon)
                    .font(.system(size: 24))
                Text(type.rawValue)
                    .font(.caption)
                    .multilineTextAlignment(.center)
                    .lineLimit(2)
            }
            .foregroundColor(isSelected ? .white : type.color)
            .frame(maxWidth: .infinity)
            .padding(.vertical, 16)
            .padding(.horizontal, 8)
            .background(isSelected ? type.color : .ultraThinMaterial)
            .cornerRadius(16)
            .overlay(
                RoundedRectangle(cornerRadius: 16)
                    .stroke(isSelected ? type.color : Color.clear, lineWidth: 2)
            )
        }
    }
}

extension Double {
    func normalized() -> String {
        String(format: "%.4f", self)
    }
}
