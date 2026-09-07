package handlers

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"

	"github.com/xanstomper/wheredaflock/backend/internal/models"
	"github.com/xanstomper/wheredaflock/backend/internal/store"
)

func setupTestMux(t *testing.T) (*http.ServeMux, store.SpatialStore) {
	ms, err := store.NewMemoryStore("")
	if err != nil {
		t.Fatalf("Failed to create memory store: %v", err)
	}

	handler := NewAPIHandler(ms)
	mux := http.NewServeMux()
	handler.RegisterRoutes(mux)

	return mux, ms
}

func TestHandleHealth(t *testing.T) {
	mux, _ := setupTestMux(t)

	req := httptest.NewRequest("GET", "/health", nil)
	w := httptest.NewRecorder()
	mux.ServeHTTP(w, req)

	if w.Code != http.StatusOK {
		t.Fatalf("Expected status 200, got %d", w.Code)
	}

	var resp map[string]any
	if err := json.Unmarshal(w.Body.Bytes(), &resp); err != nil {
		t.Fatalf("Failed to parse JSON response: %v", err)
	}

	if resp["status"] != "healthy" {
		t.Errorf("Expected status 'healthy', got %v", resp["status"])
	}
}

func TestHandleCameras(t *testing.T) {
	mux, ms := setupTestMux(t)

	// Seed camera
	ms.AddCamera(models.Camera{
		ID: "cam-101",
		Coordinate: models.Coordinate{
			Latitude:  35.5,
			Longitude: -78.5,
		},
		Type: models.CameraTypeFlock,
	})

	// Query without params -> 400 Bad Request
	req := httptest.NewRequest("GET", "/v1/cameras", nil)
	w := httptest.NewRecorder()
	mux.ServeHTTP(w, req)
	if w.Code != http.StatusBadRequest {
		t.Errorf("Expected 400 Bad Request, got %d", w.Code)
	}

	// Query with coordinates
	req = httptest.NewRequest("GET", "/v1/cameras?lat=35.5&lng=-78.5&radius=500", nil)
	w = httptest.NewRecorder()
	mux.ServeHTTP(w, req)

	if w.Code != http.StatusOK {
		t.Errorf("Expected 200 OK, got %d", w.Code)
	}

	var data struct {
		Count   int             `json:"count"`
		Cameras []models.Camera `json:"cameras"`
	}
	if err := json.Unmarshal(w.Body.Bytes(), &data); err != nil {
		t.Fatalf("Failed to parse cameras JSON: %v", err)
	}

	if data.Count != 1 || len(data.Cameras) != 1 {
		t.Errorf("Expected 1 camera, got %d", data.Count)
	}
}

func TestHandleEvaluateRoute(t *testing.T) {
	mux, ms := setupTestMux(t)

	ms.AddCamera(models.Camera{
		ID: "target-flock",
		Coordinate: models.Coordinate{
			Latitude:  34.0,
			Longitude: -84.0,
		},
		Confidence: 95.0,
	})

	body, _ := json.Marshal(models.RouteEvaluationRequest{
		Coordinates: [][2]float64{
			{34.0001, -84.0001},
			{34.0002, -84.0002},
		},
		Preference: "privacy",
	})

	req := httptest.NewRequest("POST", "/v1/routes/evaluate", bytes.NewReader(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	mux.ServeHTTP(w, req)

	if w.Code != http.StatusOK {
		t.Fatalf("Expected 200 OK, got %d", w.Code)
	}

	var evalResp models.RouteEvaluationResponse
	if err := json.Unmarshal(w.Body.Bytes(), &evalResp); err != nil {
		t.Fatalf("Failed to parse eval response: %v", err)
	}

	if evalResp.CameraExposureCount == 0 {
		t.Errorf("Expected at least 1 camera detected in route corridor")
	}
}
