import WidgetKit
import SwiftUI

// MARK: - Widget Timeline Entry
public struct PrivacyWidgetEntry: TimelineEntry {
    public let date: Date
    public let privacyScore: Double
    public let corridorRating: String
    public let nearestCameraDistanceFt: Int
    public let nearbyCameraCount: Int
    public let isGhostMode: Bool
    
    public init(
        date: Date = Date(),
        privacyScore: Double = 95.0,
        corridorRating: String = "Clean Corridor",
        nearestCameraDistanceFt: Int = 380,
        nearbyCameraCount: Int = 2,
        isGhostMode: Bool = true
    ) {
        self.date = date
        self.privacyScore = privacyScore
        self.corridorRating = corridorRating
        self.nearestCameraDistanceFt = nearestCameraDistanceFt
        self.nearbyCameraCount = nearbyCameraCount
        self.isGhostMode = isGhostMode
    }
}

// MARK: - Timeline Provider
public struct PrivacyTimelineProvider: TimelineProvider {
    public func placeholder(in context: Context) -> PrivacyWidgetEntry {
        PrivacyWidgetEntry()
    }

    public func getSnapshot(in context: Context, completion: @escaping (PrivacyWidgetEntry) -> Void) {
        completion(PrivacyWidgetEntry())
    }

    public func getTimeline(in context: Context, completion: @escaping (Timeline<PrivacyWidgetEntry>) -> Void) {
        let entry = PrivacyWidgetEntry()
        // Refresh every 15 minutes
        let nextUpdate = Calendar.current.date(byAdding: .minute, value: 15, to: Date())!
        let timeline = Timeline(entries: [entry], policy: .after(nextUpdate))
        completion(timeline)
    }
}

// MARK: - Widget View Layouts
public struct WhereDaFlockWidgetEntryView: View {
    var entry: PrivacyTimelineProvider.Entry
    @Environment(\.widgetFamily) var family

    public var body: some View {
        switch family {
        case .systemSmall:
            smallWidgetView
        case .systemMedium:
            mediumWidgetView
        case .accessoryCircular:
            accessoryCircularView
        case .accessoryInline:
            accessoryInlineView
        default:
            smallWidgetView
        }
    }

    // Small Home Screen Widget
    private var smallWidgetView: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack {
                Image(systemName: "shield.checkered")
                    .foregroundColor(.blue)
                Spacer()
                if entry.isGhostMode {
                    Image(systemName: "eye.slash.fill")
                        .foregroundColor(.green)
                        .font(.caption2)
                }
            }
            Spacer()
            Text("\(Int(entry.privacyScore))")
                .font(.system(size: 36, weight: .bold, design: .rounded))
                .foregroundColor(.primary)
            Text("Privacy Score")
                .font(.caption2)
                .foregroundColor(.secondary)
            HStack {
                Circle()
                    .fill(entry.nearbyCameraCount > 0 ? Color.orange : Color.green)
                    .frame(width: 6, height: 6)
                Text("\(entry.nearbyCameraCount) ALPR nearby")
                    .font(.system(size: 11, weight: .medium))
                    .foregroundColor(.secondary)
            }
        }
        .padding()
        .containerBackground(Color(.systemBackground), for: .widget)
    }

    // Medium Home Screen Widget
    private var mediumWidgetView: some View {
        HStack(spacing: 16) {
            VStack(alignment: .leading, spacing: 4) {
                HStack {
                    Image(systemName: "shield.checkered")
                        .foregroundColor(.blue)
                    Text("WhereDaFlock")
                        .font(.caption)
                        .fontWeight(.semibold)
                }
                Spacer()
                Text("\(Int(entry.privacyScore))%")
                    .font(.system(size: 32, weight: .bold, design: .rounded))
                Text(entry.corridorRating)
                    .font(.caption2)
                    .foregroundColor(.secondary)
            }
            .frame(maxWidth: .infinity, alignment: .leading)

            Divider()

            VStack(alignment: .leading, spacing: 6) {
                Label("Surveillance Radar", systemImage: "antenna.radiowaves.left.and.right")
                    .font(.caption2)
                    .foregroundColor(.secondary)
                HStack {
                    Text("Nearest ALPR:")
                        .font(.caption)
                        .foregroundColor(.secondary)
                    Spacer()
                    Text("\(entry.nearestCameraDistanceFt) ft")
                        .font(.caption)
                        .fontWeight(.bold)
                        .foregroundColor(.orange)
                }
                HStack {
                    Text("Ghost Mode:")
                        .font(.caption)
                        .foregroundColor(.secondary)
                    Spacer()
                    Text(entry.isGhostMode ? "Active" : "Off")
                        .font(.caption)
                        .fontWeight(.bold)
                        .foregroundColor(entry.isGhostMode ? .green : .secondary)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
        .padding()
        .containerBackground(Color(.systemBackground), for: .widget)
    }

    // Lock Screen Circular Complication
    private var accessoryCircularView: some View {
        Gauge(value: entry.privacyScore, in: 0...100) {
            Image(systemName: "shield.fill")
        } currentValueLabel: {
            Text("\(Int(entry.privacyScore))")
                .font(.caption2)
        }
        .gaugeStyle(.accessoryCircular)
    }

    // Lock Screen Inline Complication
    private var accessoryInlineView: some View {
        Text("🛡️ \(Int(entry.privacyScore))% | ALPR \(entry.nearestCameraDistanceFt)ft")
    }
}

// MARK: - Widget Definition
public struct WhereDaFlockWidget: Widget {
    public let kind: String = "WhereDaFlockWidget"

    public init() {}

    public var body: some WidgetConfiguration {
        StaticConfiguration(kind: kind, provider: PrivacyTimelineProvider()) { entry in
            WhereDaFlockWidgetEntryView(entry: entry)
        }
        .configurationDisplayName("WhereDaFlock Radar")
        .description("Real-time privacy corridor score and nearest ALPR camera distance.")
        .supportedFamilies([
            .systemSmall,
            .systemMedium,
            .accessoryCircular,
            .accessoryInline
        ])
    }
}

// MARK: - Widget Bundle
@main
public struct WhereDaFlockWidgetBundle: WidgetBundle {
    public init() {}

    public var body: some Widget {
        WhereDaFlockWidget()
    }
}
