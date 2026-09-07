import SwiftUI
import MapKit

struct MapViewRepresentable: UIViewRepresentable {
    @Binding var region: MKCoordinateRegion
    var userLocation: CLLocation?
    var cameras: [Camera]
    var reports: [UserReport]
    @Binding var selectedCamera: Camera?
    @Binding var selectedReport: UserReport?
    var showCameraLayer: Bool
    var showReportLayer: Bool
    var showTrafficLayer: Bool
    
    func makeUIView(context: Context) -> MKMapView {
        let mapView = MKMapView()
        mapView.delegate = context.coordinator
        mapView.showsUserLocation = true
        mapView.showsCompass = true
        mapView.showsScale = true
        mapView.showsTraffic = showTrafficLayer
        mapView.userTrackingMode = .follow
        mapView.setRegion(region, animated: false)
        return mapView
    }
    
    func updateUIView(_ mapView: MKMapView, context: Context) {
        mapView.showsTraffic = showTrafficLayer
        updateAnnotations(mapView: mapView)
    }
    
    private func updateAnnotations(mapView: MKMapView) {
        // Remove old annotations
        let oldCameras = mapView.annotations.compactMap { $0 as? CameraAnnotation }
        let oldReports = mapView.annotations.compactMap { $0 as? ReportAnnotation }
        
        if showCameraLayer {
            mapView.removeAnnotations(oldCameras)
            let newCameraAnnotations = cameras.map { c -> CameraAnnotation in
                let a = CameraAnnotation(camera: c)
                a.coordinate = c.coordinate
                a.title = c.type.rawValue
                a.subtitle = "\(Int(c.confidence))%"
                return a
            }
            mapView.addAnnotations(newCameraAnnotations)
        } else {
            mapView.removeAnnotations(oldCameras)
        }
        
        if showReportLayer {
            mapView.removeAnnotations(oldReports)
            let newReportAnnotations = reports.map { r -> ReportAnnotation in
                let a = ReportAnnotation(report: r)
                a.coordinate = r.coordinate
                a.title = r.type.rawValue
                a.subtitle = r.timestamp.timeAgo()
                return a
            }
            mapView.addAnnotations(newReportAnnotations)
        } else {
            mapView.removeAnnotations(oldReports)
        }
    }
    
    func makeCoordinator() -> Coordinator {
        Coordinator(self)
    }
    
    class Coordinator: NSObject, MKMapViewDelegate {
        var parent: MapViewRepresentable
        
        init(_ parent: MapViewRepresentable) {
            self.parent = parent
        }
        
        func mapView(_ mapView: MKMapView, viewFor annotation: MKAnnotation) -> MKAnnotationView? {
            guard !(annotation is MKUserLocation) else { return nil }
            
            if let camAnn = annotation as? CameraAnnotation {
                let id = "CameraAnnotation"
                let view = (mapView.dequeueReusableAnnotationView(withIdentifier: id) as? MKMarkerAnnotationView)
                    ?? MKMarkerAnnotationView(annotation: annotation, reuseIdentifier: id)
                view.markerTintColor = UIColor(camAnn.camera.type.color)
                view.glyphImage = UIImage(systemName: camAnn.camera.type.icon)
                view.canShowCallout = true
                view.rightCalloutAccessoryView = UIButton(type: .detailDisclosure)
                return view
            }
            
            if let rptAnn = annotation as? ReportAnnotation {
                let id = "ReportAnnotation"
                let view = (mapView.dequeueReusableAnnotationView(withIdentifier: id) as? MKMarkerAnnotationView)
                    ?? MKMarkerAnnotationView(annotation: annotation, reuseIdentifier: id)
                view.markerTintColor = UIColor(rptAnn.report.type.color)
                view.glyphImage = UIImage(systemName: rptAnn.report.type.icon)
                view.canShowCallout = true
                return view
            }
            
            return nil
        }
        
        func mapView(_ mapView: MKMapView, annotationView view: MKAnnotationView, calloutAccessoryControlTapped control: UIControl) {
            if let camAnn = view.annotation as? CameraAnnotation {
                parent.selectedCamera = camAnn.camera
            }
        }
        
        func mapView(_ mapView: MKMapView, regionDidChangeAnimated animated: Bool) {
            parent.region = mapView.region
        }
    }
}

class CameraAnnotation: MKPointAnnotation {
    let camera: Camera
    init(camera: Camera) { self.camera = camera; super.init() }
}

class ReportAnnotation: MKPointAnnotation {
    let report: UserReport
    init(report: UserReport) { self.report = report; super.init() }
}
