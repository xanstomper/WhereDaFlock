import Foundation
import CoreLocation
import Combine

class ReportService: ObservableObject {
    @Published var reports: [UserReport] = []
    @Published var isLoading: Bool = false
    
    func submitReport(type: ReportType, coordinate: CLLocationCoordinate2D, description: String? = nil) {
        let report = UserReport(
            id: UUID().uuidString,
            type: type,
            coordinate: coordinate,
            description: description,
            timestamp: Date(),
            expiryDate: Date().addingTimeInterval(24 * 3600), // 24 hours
            confidence: 50,
            upvotes: 0,
            downvotes: 0,
            isVerified: false,
            isAnonyous: true
        )
        
        DispatchQueue.main.async { [weak self] in
            self?.reports.append(report)
            self?.reports.sort { $0.timestamp > $1.timestamp }
        }
        
        // In production: send to backend
    }
    
    func reportsNear(location: CLLocation, radiusMeters: Double = 1000) -> [UserReport] {
        return reports.filter { report in
            let reportLocation = CLLocation(latitude: report.coordinate.latitude, longitude: report.coordinate.longitude)
            let distance = location.distance(from: reportLocation)
            return distance <= radiusMeters && report.expiryDate > Date()
        }
    }
    
    func upvote(reportId: String) {
        guard let index = reports.firstIndex(where: { $0.id == reportId }) else { return }
        var report = reports[index]
        report.upvotes += 1
        reports[index] = report
    }
    
    func downvote(reportId: String) {
        guard let index = reports.firstIndex(where: { $0.id == reportId }) else { return }
        var report = reports[index]
        report.downvotes += 1
        reports[index] = report
    }
    
    func cleanExpiredReports() {
        reports.removeAll { $0.expiryDate < Date() }
    }
}
