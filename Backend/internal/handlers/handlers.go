package handlers

import (
	"encoding/json"
	"fmt"
	"net/http"
	"strconv"
	"time"

	"github.com/xanstomper/wheredaflock/backend/internal/deflock"
	"github.com/xanstomper/wheredaflock/backend/internal/models"
	"github.com/xanstomper/wheredaflock/backend/internal/store"
)

type APIHandler struct {
	store store.SpatialStore
}

func NewAPIHandler(s store.SpatialStore) *APIHandler {
	return &APIHandler{store: s}
}

func (h *APIHandler) RegisterRoutes(mux *http.ServeMux) {
	mux.HandleFunc("GET /health", h.HandleHealth)
	mux.HandleFunc("GET /v1/cameras", h.HandleGetCameras)
	mux.HandleFunc("POST /v1/cameras", h.HandleAddCamera)
	mux.HandleFunc("GET /v1/reports", h.HandleGetReports)
	mux.HandleFunc("POST /v1/reports", h.HandleAddReport)
	mux.HandleFunc("POST /v1/reports/vote", h.HandleVoteReport)
	mux.HandleFunc("POST /v1/routes/evaluate", h.HandleEvaluateRoute)
	mux.HandleFunc("POST /v1/sync/deflock", h.HandleSyncDeFlock)
}

func jsonResponse(w http.ResponseWriter, status int, data any) {
	w.Header().Set("Content-Type", "application/json")
	w.Header().Set("Access-Control-Allow-Origin", "*")
	w.Header().Set("Access-Control-Allow-Headers", "Content-Type, Authorization")
	w.WriteHeader(status)
	json.NewEncoder(w).Encode(data)
}

func (h *APIHandler) HandleHealth(w http.ResponseWriter, r *http.Request) {
	jsonResponse(w, http.StatusOK, map[string]any{
		"status":        "healthy",
		"total_cameras": h.store.TotalCameras(),
		"service":       "WhereDaFlock Spatial API",
		"version":       "1.0.0",
		"timestamp":     time.Now().UTC().Format(time.RFC3339),
	})
}

func (h *APIHandler) HandleGetCameras(w http.ResponseWriter, r *http.Request) {
	q := r.URL.Query()
	latStr := q.Get("lat")
	lonStr := q.Get("lng")
	if lonStr == "" {
		lonStr = q.Get("lon")
	}
	radiusStr := q.Get("radius")

	if latStr == "" || lonStr == "" {
		jsonResponse(w, http.StatusBadRequest, map[string]string{
			"error": "Query parameters 'lat' and 'lng' are required",
		})
		return
	}

	lat, err1 := strconv.ParseFloat(latStr, 64)
	lon, err2 := strconv.ParseFloat(lonStr, 64)
	if err1 != nil || err2 != nil {
		jsonResponse(w, http.StatusBadRequest, map[string]string{
			"error": "Invalid float values for 'lat' or 'lng'",
		})
		return
	}

	radiusMeters := 1000.0
	if radiusStr != "" {
		if rVal, err := strconv.ParseFloat(radiusStr, 64); err == nil && rVal > 0 {
			radiusMeters = rVal
		}
	}

	cams, err := h.store.GetCamerasNear(lat, lon, radiusMeters)
	if err != nil {
		jsonResponse(w, http.StatusInternalServerError, map[string]string{"error": err.Error()})
		return
	}

	jsonResponse(w, http.StatusOK, map[string]any{
		"count":   len(cams),
		"radius":  radiusMeters,
		"cameras": cams,
	})
}

func (h *APIHandler) HandleAddCamera(w http.ResponseWriter, r *http.Request) {
	var cam models.Camera
	if err := json.NewDecoder(r.Body).Decode(&cam); err != nil {
		jsonResponse(w, http.StatusBadRequest, map[string]string{"error": "Invalid JSON payload"})
		return
	}

	if cam.ID == "" {
		cam.ID = fmt.Sprintf("cam-user-%d", time.Now().UnixNano())
	}
	if cam.LastVerified.IsZero() {
		cam.LastVerified = time.Now()
	}

	if err := h.store.AddCamera(cam); err != nil {
		jsonResponse(w, http.StatusInternalServerError, map[string]string{"error": err.Error()})
		return
	}

	jsonResponse(w, http.StatusCreated, map[string]any{
		"status": "created",
		"camera": cam,
	})
}

func (h *APIHandler) HandleGetReports(w http.ResponseWriter, r *http.Request) {
	q := r.URL.Query()
	latStr := q.Get("lat")
	lonStr := q.Get("lng")
	if lonStr == "" {
		lonStr = q.Get("lon")
	}
	radiusStr := q.Get("radius")

	if latStr == "" || lonStr == "" {
		jsonResponse(w, http.StatusBadRequest, map[string]string{
			"error": "Query parameters 'lat' and 'lng' are required",
		})
		return
	}

	lat, _ := strconv.ParseFloat(latStr, 64)
	lon, _ := strconv.ParseFloat(lonStr, 64)
	radiusMeters := 5000.0
	if radiusStr != "" {
		if rVal, err := strconv.ParseFloat(radiusStr, 64); err == nil && rVal > 0 {
			radiusMeters = rVal
		}
	}

	reps, err := h.store.GetActiveReports(lat, lon, radiusMeters)
	if err != nil {
		jsonResponse(w, http.StatusInternalServerError, map[string]string{"error": err.Error()})
		return
	}

	jsonResponse(w, http.StatusOK, map[string]any{
		"count":   len(reps),
		"reports": reps,
	})
}

func (h *APIHandler) HandleAddReport(w http.ResponseWriter, r *http.Request) {
	var rep models.Report
	if err := json.NewDecoder(r.Body).Decode(&rep); err != nil {
		jsonResponse(w, http.StatusBadRequest, map[string]string{"error": "Invalid JSON payload"})
		return
	}

	if rep.ID == "" {
		rep.ID = fmt.Sprintf("rep-%d", time.Now().UnixNano())
	}
	rep.Timestamp = time.Now()
	rep.ExpiryDate = rep.Timestamp.Add(24 * time.Hour)
	if rep.Confidence == 0 {
		rep.Confidence = 80.0
	}
	rep.Upvotes = 1

	if err := h.store.AddReport(rep); err != nil {
		jsonResponse(w, http.StatusInternalServerError, map[string]string{"error": err.Error()})
		return
	}

	jsonResponse(w, http.StatusCreated, map[string]any{
		"status": "created",
		"report": rep,
	})
}

func (h *APIHandler) HandleVoteReport(w http.ResponseWriter, r *http.Request) {
	type VoteRequest struct {
		ReportID string `json:"report_id"`
		Upvote   bool   `json:"upvote"`
	}
	var vote VoteRequest
	if err := json.NewDecoder(r.Body).Decode(&vote); err != nil {
		jsonResponse(w, http.StatusBadRequest, map[string]string{"error": "Invalid vote payload"})
		return
	}

	if err := h.store.VoteReport(vote.ReportID, vote.Upvote); err != nil {
		jsonResponse(w, http.StatusNotFound, map[string]string{"error": err.Error()})
		return
	}

	jsonResponse(w, http.StatusOK, map[string]string{"status": "vote recorded"})
}

func (h *APIHandler) HandleEvaluateRoute(w http.ResponseWriter, r *http.Request) {
	var req models.RouteEvaluationRequest
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		jsonResponse(w, http.StatusBadRequest, map[string]string{"error": "Invalid route payload"})
		return
	}

	resp := h.store.EvaluateRoute(req)
	jsonResponse(w, http.StatusOK, resp)
}

func (h *APIHandler) HandleSyncDeFlock(w http.ResponseWriter, r *http.Request) {
	bbox := r.URL.Query().Get("bbox")
	cams, err := deflock.FetchOSMALPR(bbox)
	if err != nil {
		jsonResponse(w, http.StatusBadGateway, map[string]string{
			"error": fmt.Sprintf("OSM DeFlock sync error: %v", err),
		})
		return
	}

	count := 0
	for _, cam := range cams {
		if err := h.store.AddCamera(cam); err == nil {
			count++
		}
	}

	jsonResponse(w, http.StatusOK, map[string]any{
		"status":        "synced",
		"fetched_count": len(cams),
		"added_count":   count,
		"total_cameras": h.store.TotalCameras(),
	})
}
