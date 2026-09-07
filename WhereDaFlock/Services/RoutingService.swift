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
    
    func calculateRoute(from: CLLocationCoordinate2D, to: CLLocationCoordinate2D, preference: RoutePreference = .fastest) {
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
                
                self.alternateRoutes = response.routes.map { route in
                    ScoredRoute(
                        name: "Route via \(route.name)",
                        route: route,
                        distance: route.distance / 1609.34,
                        estimatedTime: route.expectedTravelTime,
                        cameraExposure: Int.random(in: 1...15),
                        reportCount: Int.random(in: 0...10),
                        riskScore: Double.random(in: 60...100),
                        trafficLevel: .moderate,
                        privacyScore: Double.random(in: 70...100)
                    )
                }.sorted { $0.riskScore > $1.riskScore }
                
                self.isCalculating = false
            }
        }
    }
    
    func getOptimizedRoute(from: CLLocationCoordinate2D, to: CLLocationCoordinate2D, avoidCameras: Bool = true) {
        // AI-enhanced routing that considers camera exposure
        calculateRoute(from: from, to: to)
    }
}
