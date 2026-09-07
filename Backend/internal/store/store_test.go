package store

import (
	"math"
	"testing"
	"time"

	"github.com/xanstomper/wheredaflock/backend/internal/models"
)

func TestHaversineMeters(t *testing.T) {
	// Distance to self should be 0
	d0 := HaversineMeters(37.7749, -122.4194, 37.7749, -122.4194)
	if d0 > 0.001 {
		t.Errorf("Expected distance to self to be 0, got %f", d0)
	}

	// SF to Oakland is approx 12-15km
	dSFtoOakland := HaversineMeters(37.7749, -122.4194, 37.8044, -122.2712)
	if dSFtoOakland < 10000 || dSFtoOakland > 16000 {
		t.Errorf("Unexpected SF to Oakland distance: %f meters", dSFtoOakland)
	}

	// 1 degree latitude at equator is approx 111,139 meters
	dLat := HaversineMeters(0, 0, 1.0, 0)
	if math.Abs(dLat-111195) > 1000 {
		t.Errorf("1 deg latitude should be approx 111.1km, got %f", dLat)
	}
}

func TestMemoryStoreCameras(t *testing.T) {
	ms, err := NewMemoryStore("")
	if err != nil {
		t.Fatalf("Failed to create memory store: %v", err)
	}

	c1 := models.Camera{
		ID:   "test-cam-1",
		Type: models.CameraTypeFlock,
		Coordinate: models.Coordinate{
			Latitude:  34.0522,
			Longitude: -118.2437,
		},
		Owner:      "Flock Safety",
		Confidence: 95.0,
	}

	c2 := models.Camera{
		ID:   "test-cam-2",
		Type: models.CameraTypeTraffic,
		Coordinate: models.Coordinate{
			Latitude:  34.1000,
			Longitude: -118.2437,
		},
		Owner:      "City DOT",
		Confidence: 90.0,
	}

	if err := ms.AddCamera(c1); err != nil {
		t.Fatalf("Failed to add camera 1: %v", err)
	}
	if err := ms.AddCamera(c2); err != nil {
		t.Fatalf("Failed to add camera 2: %v", err)
	}

	if ms.TotalCameras() != 2 {
		t.Errorf("Expected 2 total cameras, got %d", ms.TotalCameras())
	}

	// Query near c1 with 500m radius - should only return c1
	nearby, err := ms.GetCamerasNear(34.0522, -118.2437, 500)
	if err != nil {
		t.Fatalf("Error querying nearby cameras: %v", err)
	}
	if len(nearby) != 1 || nearby[0].ID != "test-cam-1" {
		t.Errorf("Expected only test-cam-1, got %d results", len(nearby))
	}

	// Query with 10km radius - should return both
	nearbyAll, err := ms.GetCamerasNear(34.0522, -118.2437, 10000)
	if err != nil {
		t.Fatalf("Error querying 10km cameras: %v", err)
	}
	if len(nearbyAll) != 2 {
		t.Errorf("Expected 2 cameras within 10km, got %d", len(nearbyAll))
	}
}

func TestMemoryStoreReportsAndVoting(t *testing.T) {
	ms, _ := NewMemoryStore("")

	rep := models.Report{
		ID: "rep-101",
		Coordinate: models.Coordinate{
			Latitude:  36.0,
			Longitude: -78.0,
		},
		Type:        models.ReportTypeCamera,
		Description: "Newly installed Falcon on solar pole",
		Timestamp:   time.Now().UTC(),
		ExpiryDate:  time.Now().UTC().Add(24 * time.Hour),
		Confidence:  50.0,
	}

	if err := ms.AddReport(rep); err != nil {
		t.Fatalf("Failed to add report: %v", err)
	}

	active, err := ms.GetActiveReports(36.0, -78.0, 1000)
	if err != nil {
		t.Fatalf("Failed to get active reports: %v", err)
	}
	if len(active) != 1 {
		t.Fatalf("Expected 1 active report, got %d", len(active))
	}

	// Upvote report
	if err := ms.VoteReport("rep-101", true); err != nil {
		t.Fatalf("Failed to upvote: %v", err)
	}
	active, _ = ms.GetActiveReports(36.0, -78.0, 1000)
	if active[0].Upvotes != 1 {
		t.Errorf("Expected 1 upvote, got %d", active[0].Upvotes)
	}

	// Downvote report
	if err := ms.VoteReport("rep-101", false); err != nil {
		t.Fatalf("Failed to downvote: %v", err)
	}
	active, _ = ms.GetActiveReports(36.0, -78.0, 1000)
	if active[0].Downvotes != 1 {
		t.Errorf("Expected 1 downvote, got %d", active[0].Downvotes)
	}
}

func TestRouteEvaluation(t *testing.T) {
	ms, _ := NewMemoryStore("")

	// Add a camera at (36.0005, -78.0005)
	ms.AddCamera(models.Camera{
		ID: "route-cam",
		Coordinate: models.Coordinate{
			Latitude:  36.0005,
			Longitude: -78.0005,
		},
		Confidence: 95.0,
	})

	// Waypoints passing right next to the camera (< 80m)
	coords := [][2]float64{
		{36.0000, -78.0000},
		{36.0010, -78.0010},
	}

	res := ms.EvaluateRoute(models.RouteEvaluationRequest{
		Coordinates: coords,
		Preference:  "privacy",
	})

	if res.CameraExposureCount == 0 {
		t.Errorf("Expected route evaluation to detect camera near polyline")
	}
	if res.CorridorRating == "Clean Corridor" {
		t.Errorf("Expected non-clean corridor rating when exposed to camera, got %s", res.CorridorRating)
	}
	if res.PrivacyScore >= 100 {
		t.Errorf("Expected privacy penalty when exposed to camera, got %f", res.PrivacyScore)
	}
}
