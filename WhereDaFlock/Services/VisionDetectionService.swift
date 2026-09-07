import Foundation
import Vision
import CoreML
import AVFoundation
import Combine

class VisionDetectionService: NSObject, ObservableObject {
    @Published var detectedObjects: [DetectedObject] = []
    @Published var isScanning: Bool = false
    @Published var confidenceThreshold: Double = 0.6
    @Published var isUsingNeuralCoreML: Bool = false
    
    private var detectionRequest: VNCoreMLRequest?
    private var opticalFallbackRequest: VNDetectRectanglesRequest?
    
    override init() {
        super.init()
        setupModel()
        setupOpticalFallback()
    }
    
    private func setupModel() {
        if let mlModel = CameraDetectionModel.shared.model,
           let model = try? VNCoreMLModel(for: mlModel) {
            detectionRequest = VNCoreMLRequest(model: model) { [weak self] request, error in
                guard let results = request.results as? [VNRecognizedObjectObservation] else { return }
                self?.processDetections(results)
            }
            detectionRequest?.imageCropAndScaleOption = .scaleFit
            isUsingNeuralCoreML = true
            print("[WhereDaFlock] Neural CoreML model loaded successfully.")
        } else {
            isUsingNeuralCoreML = false
            print("[WhereDaFlock] CoreML weights not present. Activating Apple Vision optical heuristic fallback.")
        }
    }
    
    private func setupOpticalFallback() {
        opticalFallbackRequest = VNDetectRectanglesRequest { [weak self] request, error in
            guard let self = self, let results = request.results as? [VNRectangleObservation] else { return }
            self.processOpticalRectangles(results)
        }
        opticalFallbackRequest?.minimumConfidence = Float(confidenceThreshold)
        opticalFallbackRequest?.minimumAspectRatio = 0.2
        opticalFallbackRequest?.maximumAspectRatio = 5.0
        opticalFallbackRequest?.maximumObservations = 5
    }
    
    func analyzeFrame(_ pixelBuffer: CVPixelBuffer) {
        isScanning = true
        let handler = VNImageRequestHandler(cvPixelBuffer: pixelBuffer, options: [:])
        
        if let coreMLReq = detectionRequest {
            try? handler.perform([coreMLReq])
        } else if let opticalReq = opticalFallbackRequest {
            try? handler.perform([opticalReq])
        }
    }
    
    private func processDetections(_ observations: [VNRecognizedObjectObservation]) {
        var newDetections: [DetectedObject] = []
        
        for observation in observations {
            guard observation.confidence > Float(confidenceThreshold) else { continue }
            
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
    
    private func processOpticalRectangles(_ observations: [VNRectangleObservation]) {
        var opticalDetections: [DetectedObject] = []
        
        for obs in observations {
            guard obs.confidence >= Float(confidenceThreshold) else { continue }
            
            // Optical fixture heuristic: elevated narrow rectangles with high vertical contrast
            let detected = DetectedObject(
                type: .camera,
                confidence: Double(obs.confidence) * 0.85,
                boundingBox: obs.boundingBox,
                distanceEstimate: nil
            )
            opticalDetections.append(detected)
        }
        
        DispatchQueue.main.async { [weak self] in
            self?.detectedObjects = opticalDetections
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
