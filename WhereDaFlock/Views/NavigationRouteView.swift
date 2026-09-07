import SwiftUI
import MapKit

struct NavigationRouteView: View {
    @EnvironmentObject var locationService: LocationService
    @StateObject private var routingService = RoutingService()
    @StateObject private var cameraService = CameraDatabaseService()
    @State private var destinationText: String = ""
    @State private var selectedPreference: RoutePreference = .fastest
    @State private var showRouteOptions: Bool = false
    @State private var isNavigating: Bool = false
    
    var body: some View {
        NavigationStack {
            ZStack {
                if isNavigating, let route = routingService.currentRoute {
                    ActiveNavigationView(
                        route: route,
                        steps: routingService.stepByStepDirections,
                        onEnd: { isNavigating = false }
                    )
                } else {
                    VStack(spacing: 0) {
                        SearchAndPreferencesView(
                            destinationText: $destinationText,
                            selectedPreference: $selectedPreference
                        )
                        
                        MapViewPreview(
                            route: routingService.currentRoute,
                            alternateRoutes: routingService.alternateRoutes
                        )
                        .cornerRadius(16)
                        .padding(.horizontal)
                        
                        if showRouteOptions && !routingService.alternateRoutes.isEmpty {
                            RouteSelectionList(routes: routingService.alternateRoutes) { scoredRoute in
                                routingService.currentRoute = scoredRoute.route
                                isNavigating = true
                                showRouteOptions = false
                            }
                            .padding()
                        }
                        
                        Spacer()
                        
                        if !destinationText.isEmpty {
                            Button(action: searchRoute) {
                                HStack {
                                    Image(systemName: "location.north.line.fill")
                                    Text("Find Route").fontWeight(.semibold)
                                }
                                .frame(maxWidth: .infinity)
                                .padding()
                                .background(Color.blue.gradient)
                                .foregroundColor(.white)
                                .cornerRadius(16)
                                .padding()
                            }
                        }
                    }
                }
            }
            .navigationTitle("Navigate")
            .navigationBarTitleDisplayMode(.large)
        }
    }
    
    private func searchRoute() {
        guard let userLocation = locationService.userLocation else { return }
        let destLat = userLocation.coordinate.latitude + 0.01
        let destLng = userLocation.coordinate.longitude + 0.01
        routingService.calculateRoute(
            from: userLocation.coordinate,
            to: dest,
            preference: selectedPreference,
            cameras: cameraService.cameras
        )
        showRouteOptions = true
    }
}
