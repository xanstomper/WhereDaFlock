import Foundation
import Vision
import CoreML
import AVFoundation
import Combine

class VisionDetectionService: NSObject, ObservableObject {
    @Published var detectedObjects: [DetectedObject] = []
    @Published var isScanning: Bool = false
    @Published var confidenceThreshold: Double = 0.6
    
    private var detectionRequest: VNCoreMLRequest?
    
    override init() {
        super.init()
        setupModel()
    }
    
    private func setupModel() {
        guard let mlModel = CameraDetectionModel.shared.model,
              let model = try? VNCoreMLModel(for: mlModel) else {
            print("[WhereDaFlock] Notice: CameraDetectionModel bundle not found. Vision scanner operating in passive heuristic mode.")
            return
        }
        
        detectionRequest = VNCoreMLRequest(model: model) { [weak self] request, error in
            guard let results = request.results as? [VNRecognizedObjectObservation] else { return }
            self?.processDetections(results)
        }
        
        detectionRequest?.imageCropAndScaleOption = .scaleFit
    }
    
    func analyzeFrame(_ pixelBuffer: CVPixelBuffer) {
        guard let request = detectionRequest else { return }
        isScanning = true
        
        let handler = VNImageRequestHandler(cvPixelBuffer: pixelBuffer, options: [:])
        try? handler.perform([request])
    }
    
    private func processDetections(_ observations: [VNRecognizedObjectObservation]) {
        var newDetections: [DetectedObject] = []
        
        for observation in observations {
            guard observation.confidence > confidenceThreshold else { continue }
            
            let topLabel = observation.labels.first?.identifier ?? "unknown"
            let objectType = classifyDetection(topLabel)
            
            let detected = DetectedObject(
                type: objectType,
                confidence: Double(observation.confidence),
                boundingBox: observation.boundingBox,
                distanceEstimate: nil
            )
            
            newDetections.append(detected)
        }
        
        DispatchQueue.main.async { [weak self] in
            self?.detectedObjects = newDetections
            self?.isScanning = false
        }
    }
    
    private func classifyDetection(_ label: String) -> DetectedObject.DetectedObjectType {
        let lower = label.lowercased()
        if lower.contains("alpr") || lower.contains("license plate") { return .alprCamera }
        if lower.contains("traffic camera") || lower.contains("traffic cam") { return .trafficCamera }
        if lower.contains("speed camera") || lower.contains("speed cam") { return .speedCamera }
        if lower.contains("police") || lower.contains("cruiser") || lower.contains("patrol") { return .policeVehicle }
        if lower.contains("camera") || lower.contains("cam") { return .camera }
        if lower.contains("vehicle") || lower.contains("car") || lower.contains("truck") { return .vehicle }
        if lower.contains("person") || lower.contains("pedestrian") { return .pedestrian }
        if lower.contains("sign") || lower.contains("road sign") { return .roadSign }
        return .camera
    }
}
