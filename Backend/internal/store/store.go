package store

import (
	"encoding/json"
	"errors"
	"fmt"
	"math"
	"os"
	"sync"
	"time"

	"github.com/xanstomper/wheredaflock/backend/internal/models"
)

// Haversine distance in meters between two lat/lon coordinates.
func HaversineMeters(lat1, lon1, lat2, lon2 float64) float64 {
	const R = 6371000.0 // Earth radius in meters
	dLat := (lat2 - lat1) * (math.Pi / 180.0)
	dLon := (lon2 - lon1) * (math.Pi / 180.0)
	a := math.Sin(dLat/2.0)*math.Sin(dLat/2.0) +
		math.Cos(lat1*(math.Pi/180.0))*math.Cos(lat2*(math.Pi/180.0))*
			math.Sin(dLon/2.0)*math.Sin(dLon/2.0)
	c := 2.0 * math.Atan2(math.Sqrt(a), math.Sqrt(1.0-a))
	return R * c
}

// SpatialStore interface defines camera and report management.
type SpatialStore interface {
	GetCamerasNear(lat, lon, radiusMeters float64) ([]models.Camera, error)
	AddCamera(cam models.Camera) error
	TotalCameras() int
	GetActiveReports(lat, lon, radiusMeters float64) ([]models.Report, error)
	AddReport(rep models.Report) error
	VoteReport(id string, upvote bool) error
	EvaluateRoute(req models.RouteEvaluationRequest) models.RouteEvaluationResponse
}

// MemoryStore implements SpatialStore with fast in-memory spatial scanning.
type MemoryStore struct {
	mu      sync.RWMutex
	cameras []models.Camera
	reports map[string]models.Report
}

// NewMemoryStore initializes a memory store, optionally pre-seeded from a JSON file.
func NewMemoryStore(seedPath string) (*MemoryStore, error) {
	ms := &MemoryStore{
		cameras: make([]models.Camera, 0, 16000),
		reports: make(map[string]models.Report),
	}

	if seedPath != "" {
		if data, err := os.ReadFile(seedPath); err == nil {
			var loaded []models.Camera
			if err := json.Unmarshal(data, &loaded); err == nil {
				ms.cameras = loaded
				fmt.Printf("[store] Pre-seeded %d surveillance cameras from %s\n", len(loaded), seedPath)
			} else {
				fmt.Printf("[store] Warning parsing seed file %s: %v\n", seedPath, err)
			}
		}
	}

	return ms, nil
}

func (m *MemoryStore) GetCamerasNear(lat, lon, radiusMeters float64) ([]models.Camera, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()

	var results []models.Camera
	for _, cam := range m.cameras {
		d := HaversineMeters(lat, lon, cam.Coordinate.Latitude, cam.Coordinate.Longitude)
		if d <= radiusMeters {
			results = append(results, cam)
		}
	}
	return results, nil
}

func (m *MemoryStore) AddCamera(cam models.Camera) error {
	m.mu.Lock()
	defer m.mu.Unlock()

	// Check if already present
	for i, existing := range m.cameras {
		if existing.ID == cam.ID {
			m.cameras[i] = cam
			return nil
		}
	}
	m.cameras = append(m.cameras, cam)
	return nil
}

func (m *MemoryStore) TotalCameras() int {
	m.mu.RLock()
	defer m.mu.RUnlock()
	return len(m.cameras)
}

func (m *MemoryStore) GetActiveReports(lat, lon, radiusMeters float64) ([]models.Report, error) {
	m.mu.RLock()
	defer m.mu.RUnlock()

	now := time.Now()
	var results []models.Report
	for _, rep := range m.reports {
		if rep.ExpiryDate.Before(now) {
			continue
		}
		d := HaversineMeters(lat, lon, rep.Coordinate.Latitude, rep.Coordinate.Longitude)
		if d <= radiusMeters {
			results = append(results, rep)
		}
	}
	return results, nil
}

func (m *MemoryStore) AddReport(rep models.Report) error {
	m.mu.Lock()
	defer m.mu.Unlock()

	if rep.Timestamp.IsZero() {
		rep.Timestamp = time.Now()
	}
	if rep.ExpiryDate.IsZero() {
		rep.ExpiryDate = rep.Timestamp.Add(24 * time.Hour)
	}
	m.reports[rep.ID] = rep
	return nil
}

func (m *MemoryStore) VoteReport(id string, upvote bool) error {
	m.mu.Lock()
	defer m.mu.Unlock()

	rep, ok := m.reports[id]
	if !ok {
		return errors.New("report not found")
	}

	if upvote {
		rep.Upvotes++
		rep.Confidence = math.Min(100, rep.Confidence+10.0)
	} else {
		rep.Downvotes++
		rep.Confidence = math.Max(0, rep.Confidence-15.0)
	}
	m.reports[id] = rep
	return nil
}

func (m *MemoryStore) EvaluateRoute(req models.RouteEvaluationRequest) models.RouteEvaluationResponse {
	m.mu.RLock()
	defer m.mu.RUnlock()

	if len(req.Coordinates) < 2 {
		return models.RouteEvaluationResponse{
			RiskScore:      100.0,
			PrivacyScore:   100.0,
			CorridorRating: "Safe",
		}
	}

	totalDist := 0.0
	for i := 0; i < len(req.Coordinates)-1; i++ {
		p1, p2 := req.Coordinates[i], req.Coordinates[i+1]
		totalDist += HaversineMeters(p1[0], p1[1], p2[0], p2[1])
	}

	// Corridor buffer distance: 80 meters from polyline nodes
	const bufferM = 80.0
	encounteredCameras := make(map[string]bool)
	encounteredReports := make(map[string]bool)

	for _, pt := range req.Coordinates {
		lat, lon := pt[0], pt[1]

		for _, cam := range m.cameras {
			if !encounteredCameras[cam.ID] {
				if HaversineMeters(lat, lon, cam.Coordinate.Latitude, cam.Coordinate.Longitude) <= bufferM {
					encounteredCameras[cam.ID] = true
				}
			}
		}

		for _, rep := range m.reports {
			if !encounteredReports[rep.ID] && rep.ExpiryDate.After(time.Now()) {
				if HaversineMeters(lat, lon, rep.Coordinate.Latitude, rep.Coordinate.Longitude) <= bufferM {
					encounteredReports[rep.ID] = true
				}
			}
		}
	}

	camCount := len(encounteredCameras)
	repCount := len(encounteredReports)

	camScore := math.Max(0, 100.0-float64(camCount*8))
	reportScore := math.Max(0, 100.0-float64(repCount*10))
	trafficScore := 75.0 // baseline

	compositeRisk := (camScore * 0.40) + (reportScore * 0.35) + (trafficScore * 0.25)
	privacyScore := math.Max(0, math.Min(100, 100.0-float64(camCount*12)))

	rating := "Clean Corridor"
	if camCount > 5 || compositeRisk < 50 {
		rating = "High Surveillance Density"
	} else if camCount >= 1 || compositeRisk < 80 {
		rating = "Surveillance Detected"
	}

	return models.RouteEvaluationResponse{
		TotalDistanceMeters: totalDist,
		CameraExposureCount: camCount,
		ReportCount:         repCount,
		RiskScore:           compositeRisk,
		PrivacyScore:        privacyScore,
		CorridorRating:      rating,
	}
}
