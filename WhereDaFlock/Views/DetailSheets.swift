import SwiftUI

struct CameraDetailSheet: View {
    let camera: Camera
    let onDismiss: () -> Void
    
    var body: some View {
        VStack {
            Spacer()
            VStack(alignment: .leading, spacing: 12) {
                HStack {
                    Image(systemName: camera.type.icon)
                        .font(.title2)
                        .foregroundColor(camera.type.color)
                    Text(camera.type.rawValue)
                        .font(.title3.weight(.bold))
                    Spacer()
                    Button(action: onDismiss) {
                        Image(systemName: "xmark.circle.fill")
                            .font(.title2)
                            .foregroundColor(.secondary)
                    }
                }
                Divider()
                InfoRow(label: "Location", value: camera.address ?? "Unknown")
                InfoRow(label: "Owner", value: camera.owner ?? "Unknown")
                InfoRow(label: "Confidence", value: "\(Int(camera.confidence))%")
                InfoRow(label: "Verified", value: camera.lastVerified.timeAgo())
                InfoRow(label: "Source", value: camera.source.name)
            }
            .padding()
            .background(.regularMaterial)
            .cornerRadius(20)
            .padding(.horizontal)
            .padding(.bottom, 100)
        }
    }
}

struct ReportDetailSheet: View {
    let report: UserReport
    let onDismiss: () -> Void
    
    var body: some View {
        VStack {
            Spacer()
            VStack(alignment: .leading, spacing: 12) {
                HStack {
                    Image(systemName: report.type.icon)
                        .font(.title2)
                        .foregroundColor(report.type.color)
                    Text(report.type.rawValue)
                        .font(.title3.weight(.bold))
                    Spacer()
                    Button(action: onDismiss) {
                        Image(systemName: "xmark.circle.fill")
                            .font(.title2)
                            .foregroundColor(.secondary)
                    }
                }
                Divider()
                if let desc = report.description {
                    Text(desc).font(.body)
                }
                HStack {
                    Label(report.timestamp.timeAgo(), systemImage: "clock")
                    Spacer()
                    Label("\(report.upvotes)", systemImage: "hand.thumbsup")
                    Label("\(report.downvotes)", systemImage: "hand.thumbsdown")
                }
                .font(.caption)
                .foregroundColor(.secondary)
            }
            .padding()
            .background(.regularMaterial)
            .cornerRadius(20)
            .padding(.horizontal)
            .padding(.bottom, 100)
        }
    }
}

struct InfoRow: View {
    let label: String
    let value: String
    
    var body: some View {
        HStack {
            Text(label).font(.caption).foregroundColor(.secondary)
            Spacer()
            Text(value).font(.subheadline)
        }
    }
}
