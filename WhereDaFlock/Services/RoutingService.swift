import Foundation
import CoreLocation
import MapKit
import Combine

class RoutingService: ObservableObject {
    @Published var currentRoute: MKRoute?
    @Published var alternateRoutes: [ScoredRoute] = []
    @Published var isCalculating: Bool = false
    @Published var stepByStepDirections: [MKRoute.Step] = []
    
    private var cancellables = Set<AnyCancellable>()
    
    func calculateRoute(
        from: CLLocationCoordinate2D,
        to: CLLocationCoordinate2D,
        preference: RoutePreference = .fastest,
        cameras: [Camera] = [],
        reports: [Report] = []
    ) {
        isCalculating = true
        
        let request = MKDirections.Request()
        request.source = MKMapItem(placemark: MKPlacemark(coordinate: from))
        request.destination = MKMapItem(placemark: MKPlacemark(coordinate: to))
        request.transportType = .automobile
        request.requestsAlternateRoutes = true
        
        let directions = MKDirections(request: request)
        directions.calculate { [weak self] response, error in
            guard let self = self, let response = response else {
                self?.isCalculating = false
                return
            }
            
            DispatchQueue.main.async {
                self.currentRoute = response.routes.first
                self.stepByStepDirections = response.routes.first?.steps ?? []
                
                let scoredList: [ScoredRoute] = response.routes.map { route in
                    let pointCount = route.polyline.pointCount
                    var polyCoords = [CLLocationCoordinate2D](repeating: kCLLocationCoordinate2DInvalid, count: pointCount)
                    route.polyline.getCoordinates(&polyCoords, range: NSRange(location: 0, length: pointCount))
                    
                    let evaluated = RouteScorer.score(
                        coordinates: polyCoords,
                        cameras: cameras,
                        reports: reports
                    )
                    
                    return ScoredRoute(
                        name: "Route via \(route.name)",
                        route: route,
                        distance: route.distance / 1609.34,
                        estimatedTime: route.expectedTravelTime,
                        cameraExposure: evaluated.cameraCount,
                        reportCount: evaluated.reportCount,
                        riskScore: evaluated.riskScore,
                        trafficLevel: .moderate,
                        privacyScore: evaluated.privacyScore
                    )
                }
                
                if preference == .privacy {
                    self.alternateRoutes = scoredList.sorted { $0.privacyScore > $1.privacyScore }
                } else {
                    self.alternateRoutes = scoredList.sorted { $0.estimatedTime < $1.estimatedTime }
                }
                
                self.isCalculating = false
            }
        }
    }
    
    func getOptimizedRoute(
        from: CLLocationCoordinate2D,
        to: CLLocationCoordinate2D,
        avoidCameras: Bool = true,
        cameras: [Camera] = []
    ) {
        calculateRoute(
            from: from,
            to: to,
            preference: avoidCameras ? .privacy : .fastest,
            cameras: cameras
        )
    }
}
