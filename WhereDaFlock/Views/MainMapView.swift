import SwiftUI
import MapKit

struct MainMapView: View {
    @EnvironmentObject var locationService: LocationService
    @EnvironmentObject var appState: AppState
    @StateObject private var cameraService = CameraDatabaseService()
    @StateObject private var reportService = ReportService()
    @StateObject private var viewModel = MapViewModel()
    
    var body: some View {
        ZStack {
            MapViewRepresentable(
                region: $viewModel.region,
                userLocation: locationService.userLocation,
                cameras: cameraService.cameras,
                reports: reportService.reports,
                selectedCamera: $viewModel.selectedCamera,
                selectedReport: $viewModel.selectedReport,
                showCameraLayer: viewModel.showCameraLayer,
                showReportLayer: viewModel.showReportLayer,
                showTrafficLayer: viewModel.showTrafficLayer
            )
            .edgesIgnoringSafeArea(.all)
            
            // UI Overlay
            VStack {
                TopBarView(showSearch: $viewModel.showSearch,
                          showLayerOptions: $viewModel.showLayerOptions)
                
                Spacer()
                
                BottomOverlayView(cameraService: cameraService,
                                 reportService: reportService,
                                 viewModel: viewModel)
                    .padding(.bottom, 80)
            }
            
            // Sheets
            if let camera = viewModel.selectedCamera {
                CameraDetailSheet(camera: camera) {
                    viewModel.selectedCamera = nil
                }
            }
            
            if let report = viewModel.selectedReport {
                ReportDetailSheet(report: report) {
                    viewModel.selectedReport = nil
                }
            }
            
            if viewModel.showLayerOptions {
                LayerOptionsView(showCameraLayer: $viewModel.showCameraLayer,
                               showReportLayer: $viewModel.showReportLayer,
                               showTrafficLayer: $viewModel.showTrafficLayer) {
                    viewModel.showLayerOptions = false
                }
            }
            
            if appState.showPrivacyMode {
                PrivacyDashboardView(isPresented: $appState.showPrivacyMode)
            }
        }
        .onAppear { cameraService.loadCameras() }
    }
}

class MapViewModel: ObservableObject {
    @Published var region = MKCoordinateRegion(
        center: CLLocationCoordinate2D(latitude: 36.728, longitude: -79.865),
        span: MKCoordinateSpan(latitudeDelta: 0.02, longitudeDelta: 0.02)
    )
    @Published var selectedCamera: Camera?
    @Published var selectedReport: UserReport?
    @Published var showSearch: Bool = false
    @Published var showLayerOptions: Bool = false
    @Published var showCameraLayer: Bool = true
    @Published var showReportLayer: Bool = true
    @Published var showTrafficLayer: Bool = true
}
