package models

import "time"

// CameraType defines known surveillance fixture classifications.
type CameraType string

const (
	CameraTypeFlock    CameraType = "Flock Safety Camera"
	CameraTypeALPR     CameraType = "ALPR Camera"
	CameraTypeTraffic  CameraType = "Traffic Camera"
	CameraTypeRedLight CameraType = "Red Light Camera"
	CameraTypeSpeed    CameraType = "Speed Camera"
	CameraTypePolice   CameraType = "Police Camera"
	CameraTypeUnknown  CameraType = "Unknown Camera"
)

// Coordinate represents a WGS-84 geographic point.
type Coordinate struct {
	Latitude  float64 `json:"latitude"`
	Longitude float64 `json:"longitude"`
}

// CameraSource documents the data provenance and reliability score.
type CameraSource struct {
	Name        string    `json:"name"`
	Reliability float64   `json:"reliability"`
	LastUpdated time.Time `json:"lastUpdated"`
}

// Camera represents an edge surveillance device.
type Camera struct {
	ID           string       `json:"id"`
	Type         CameraType   `json:"type"`
	Coordinate   Coordinate   `json:"coordinate"`
	Address      string       `json:"address,omitempty"`
	Owner        string       `json:"owner,omitempty"`
	LastVerified time.Time    `json:"lastVerified"`
	Confidence   float64      `json:"confidence"`
	IsConfirmed  bool         `json:"isConfirmed"`
	Source       CameraSource `json:"source"`
}

// ReportType specifies a community road alert.
type ReportType string

const (
	ReportTypePolice       ReportType = "Police Activity"
	ReportTypeCamera       ReportType = "Camera Spotted"
	ReportTypeAccident     ReportType = "Accident"
	ReportTypeHazard       ReportType = "Road Hazard"
	ReportTypeConstruction ReportType = "Construction"
	ReportTypeCheckpoint   ReportType = "Checkpoint"
	ReportTypeSpeedTrap    ReportType = "Speed Trap"
	ReportTypeRoadClosure  ReportType = "Road Closure"
	ReportTypeFlooding     ReportType = "Flooding"
	ReportTypeDebris       ReportType = "Debris on Road"
	ReportTypeAnimal       ReportType = "Animal on Road"
	ReportTypeOther        ReportType = "Other"
)

// Report represents an ephemeral anonymous community alert.
type Report struct {
	ID          string     `json:"id"`
	Type        ReportType `json:"type"`
	Coordinate  Coordinate `json:"coordinate"`
	Description string     `json:"description,omitempty"`
	Timestamp   time.Time  `json:"timestamp"`
	ExpiryDate  time.Time  `json:"expiryDate"`
	Confidence  float64    `json:"confidence"`
	Upvotes     int        `json:"upvotes"`
	Downvotes   int        `json:"downvotes"`
	IsVerified  bool       `json:"isVerified"`
	IsAnonymous bool       `json:"isAnonymous"`
}

// RouteEvaluationRequest payload submitted by navigation clients.
type RouteEvaluationRequest struct {
	Coordinates [][2]float64 `json:"coordinates"` // [latitude, longitude] tuples along polyline
	Preference  string       `json:"preference"`  // fastest, privacy, calm, scenic
}

// RouteEvaluationResponse scoring summary returned to the client.
type RouteEvaluationResponse struct {
	TotalDistanceMeters float64 `json:"total_distance_meters"`
	CameraExposureCount int     `json:"camera_exposure_count"`
	ReportCount         int     `json:"report_count"`
	RiskScore           float64 `json:"risk_score"`
	PrivacyScore        float64 `json:"privacy_score"`
	CorridorRating      string  `json:"corridor_rating"`
}
