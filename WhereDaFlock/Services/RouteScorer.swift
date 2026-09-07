import Foundation
import CoreLocation
import MapKit

public struct RouteScore: Equatable {
    public let cameraCount: Int
    public let reportCount: Int
    public let riskScore: Double
    public let privacyScore: Double
    public let corridorRating: String
    
    public init(cameraCount: Int, reportCount: Int, riskScore: Double, privacyScore: Double, corridorRating: String) {
        self.cameraCount = cameraCount
        self.reportCount = reportCount
        self.riskScore = riskScore
        self.privacyScore = privacyScore
        self.corridorRating = corridorRating
    }
}

public struct RouteScorer {
    public static let defaultCorridorBufferMeters: Double = 80.0
    
    /// Evaluates route coordinates against camera positions and reports within a corridor buffer.
    public static func score(
        coordinates: [CLLocationCoordinate2D],
        cameras: [Camera],
        reports: [Report] = [],
        bufferMeters: Double = defaultCorridorBufferMeters
    ) -> RouteScore {
        guard coordinates.count >= 2 else {
            return RouteScore(
                cameraCount: 0,
                reportCount: 0,
                riskScore: 100.0,
                privacyScore: 100.0,
                corridorRating: "Clean Corridor"
            )
        }
        
        var encounteredCameraIds = Set<String>()
        var encounteredReportIds = Set<String>()
        
        let now = Date()
        let activeReports = reports.filter { $0.expiryDate > now }
        
        for coord in coordinates {
            let pointLoc = CLLocation(latitude: coord.latitude, longitude: coord.longitude)
            
            for cam in cameras {
                if !encounteredCameraIds.contains(cam.id) {
                    let camLoc = CLLocation(latitude: cam.coordinate.latitude, longitude: cam.coordinate.longitude)
                    if pointLoc.distance(from: camLoc) <= bufferMeters {
                        encounteredCameraIds.insert(cam.id)
                    }
                }
            }
            
            for rep in activeReports {
                if !encounteredReportIds.contains(rep.id) {
                    let repLoc = CLLocation(latitude: rep.coordinate.latitude, longitude: rep.coordinate.longitude)
                    if pointLoc.distance(from: repLoc) <= bufferMeters {
                        encounteredReportIds.insert(rep.id)
                    }
                }
            }
        }
        
        let camCount = encounteredCameraIds.count
        let repCount = encounteredReportIds.count
        
        let cameraPenalty = min(100.0, Double(camCount) * 12.0)
        let reportPenalty = min(100.0, Double(repCount) * 8.0)
        
        let privacyScore = max(0.0, min(100.0, 100.0 - cameraPenalty))
        let riskScore = max(0.0, min(100.0, 100.0 - (cameraPenalty * 0.65 + reportPenalty * 0.35)))
        
        let rating: String
        if camCount > 5 || riskScore < 50.0 {
            rating = "High Surveillance Density"
        } else if camCount >= 1 || riskScore < 80.0 {
            rating = "Surveillance Detected"
        } else {
            rating = "Clean Corridor"
        }
        
        return RouteScore(
            cameraCount: camCount,
            reportCount: repCount,
            riskScore: riskScore,
            privacyScore: privacyScore,
            corridorRating: rating
        )
    }
}
