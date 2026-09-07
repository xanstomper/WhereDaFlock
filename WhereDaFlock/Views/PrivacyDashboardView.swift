import SwiftUI

struct PrivacyDashboardView: View {
    @EnvironmentObject var privacyService: PrivacyService
    @Binding var isPresented: Bool
    
    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(spacing: 20) {
                    // Privacy Score
                    VStack(spacing: 8) {
                        ZStack {
                            Circle()
                                .stroke(Color.secondary.opacity(0.2), lineWidth: 8)
                                .frame(width: 120, height: 120)
                            Circle()
                                .trim(from: 0, to: privacyService.privacyStatus.score / 100)
                                .stroke(scoreColor.gradient, lineWidth: 8)
                                .frame(width: 120, height: 120)
                                .rotationEffect(.degrees(-90))
                            Text("\(Int(privacyService.privacyStatus.score))")
                                .font(.system(size: 40, weight: .bold))
                        }
                        Text("Privacy Score").font(.headline)
                        Text("Your data stays yours").font(.caption).foregroundColor(.secondary)
                    }
                    .padding()
                    
                    // Status Items
                    GroupBox("Privacy Status") {
                        VStack(spacing: 12) {
                            StatusRow(icon: "clock.arrow.circlepath", title: "Location History",
                                     status: privacyService.privacyStatus.locationHistoryEnabled ? "On" : "Off",
                                     isEnabled: !privacyService.privacyStatus.locationHistoryEnabled)
                            StatusRow(icon: "cloud.fill", title: "Cloud Processing",
                                     status: privacyService.privacyStatus.cloudProcessingEnabled ? "On" : "Off",
                                     isEnabled: !privacyService.privacyStatus.cloudProcessingEnabled)
                            StatusRow(icon: "chart.bar.fill", title: "Analytics",
                                     status: privacyService.privacyStatus.analyticsEnabled ? "On" : "Off",
                                     isEnabled: !privacyService.privacyStatus.analyticsEnabled)
                            StatusRow(icon: "moon.fill", title: "Ghost Mode",
                                     status: privacyService.isGhostMode ? "Active" : "Inactive",
                                     isEnabled: privacyService.isGhostMode)
                            StatusRow(icon: "lock.shield.fill", title: "Encryption",
                                     status: privacyService.privacyStatus.encryptionLevel.rawValue,
                                     isEnabled: true)
                        }
                    }
                    
                    // Data
                    GroupBox("Data") {
                        VStack(spacing: 8) {
                            HStack {
                                Text("Stored Data")
                                Spacer()
                                Text("\(privacyService.privacyStatus.dataStoredBytes) bytes").foregroundColor(.secondary)
                            }
                            if let audit = privacyService.privacyStatus.lastPrivacyAudit {
                                HStack {
                                    Text("Last Audit")
                                    Spacer()
                                    Text(audit.timeAgo()).foregroundColor(.secondary)
                                }
                            }
                        }
                    }
                    
                    // Actions
                    Button(action: { privacyService.performPrivacyAudit() }) {
                        Label("Run Privacy Audit", systemImage: "checkmark.shield.fill")
                            .frame(maxWidth: .infinity).padding()
                            .background(Color.green).foregroundColor(.white).cornerRadius(16)
                    }
                    
                    Button(action: { privacyService.clearSessionData() }) {
                        Label("Clear Session Data", systemImage: "trash.fill")
                            .frame(maxWidth: .infinity).padding()
                            .background(.ultraThinMaterial).foregroundColor(.red).cornerRadius(16)
                    }
                }
                .padding()
            }
            .navigationTitle("Privacy Dashboard")
            .navigationBarTitleDisplayMode(.large)
            .toolbar {
                ToolbarItem(placement: .navigationBarTrailing) {
                    Button("Done") { isPresented = false }
                }
            }
        }
    }
    
    private var scoreColor: Color {
        switch privacyService.privacyStatus.score {
        case ..<40: return .red
        case ..<70: return .orange
        case ..<90: return .yellow
        default: return .green
        }
    }
}

struct StatusRow: View {
    let icon: String
    let title: String
    let status: String
    let isEnabled: Bool
    
    var body: some View {
        HStack {
            Image(systemName: icon).foregroundColor(isEnabled ? .green : .red).frame(width: 24)
            Text(title).font(.subheadline)
            Spacer()
            Text(status).font(.subheadline.weight(.medium)).foregroundColor(isEnabled ? .green : .secondary)
        }
    }
}
