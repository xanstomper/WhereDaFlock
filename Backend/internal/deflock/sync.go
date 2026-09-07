package deflock

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"strings"
	"time"

	"github.com/xanstomper/wheredaflock/backend/internal/models"
)

// OverpassElement represents a raw node returned by OpenStreetMap Overpass.
type OverpassElement struct {
	ID   int64             `json:"id"`
	Lat  float64           `json:"lat"`
	Lon  float64           `json:"lon"`
	Tags map[string]string `json:"tags"`
}

type OverpassResponse struct {
	Elements []OverpassElement `json:"elements"`
}

// FetchOSMALPR queries OpenStreetMap Overpass API for crowdsourced ALPR and surveillance cameras.
func FetchOSMALPR(bbox string) ([]models.Camera, error) {
	if bbox == "" {
		// Default: Continental US bounding box
		bbox = "24.0,-125.0,50.0,-66.0"
	}

	overpassURL := "https://overpass-api.de/api/interpreter"
	query := fmt.Sprintf(`[out:json][timeout:30];
(
  node["man_made"="surveillance"]["surveillance:type"="ALPR"](%s);
  node["camera:type"="alpr"](%s);
  node["surveillance"="ALPR"](%s);
);
out body 3000;`, bbox, bbox, bbox)

	formData := url.Values{"data": {query}}
	client := &http.Client{Timeout: 35 * time.Second}
	resp, err := client.Post(overpassURL, "application/x-www-form-urlencoded", strings.NewReader(formData.Encode()))
	if err != nil {
		return nil, fmt.Errorf("overpass query failed: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		bodyBytes, _ := io.ReadAll(resp.Body)
		return nil, fmt.Errorf("overpass returned status %d: %s", resp.StatusCode, string(bodyBytes))
	}

	var parsed OverpassResponse
	if err := json.NewDecoder(resp.Body).Decode(&parsed); err != nil {
		return nil, fmt.Errorf("error decoding overpass response: %w", err)
	}

	now := time.Now()
	var cameras []models.Camera
	for _, el := range parsed.Elements {
		operator := el.Tags["operator"]
		if operator == "" {
			operator = el.Tags["surveillance:operator"]
		}
		if operator == "" {
			operator = "Public Authority / ALPR"
		}

		cType := models.CameraTypeALPR
		if strings.Contains(strings.ToLower(operator), "flock") {
			cType = models.CameraTypeFlock
		}

		desc := el.Tags["name"]
		if desc == "" {
			desc = el.Tags["description"]
		}
		if desc == "" {
			desc = fmt.Sprintf("OSM ALPR Node #%d", el.ID)
		}

		cameras = append(cameras, models.Camera{
			ID:   fmt.Sprintf("osm-%d", el.ID),
			Type: cType,
			Coordinate: models.Coordinate{
				Latitude:  el.Lat,
				Longitude: el.Lon,
			},
			Address:      desc,
			Owner:        operator,
			LastVerified: now,
			Confidence:   95.0,
			IsConfirmed:  true,
			Source: models.CameraSource{
				Name:        "OpenStreetMap / DeFlock",
				Reliability: 0.95,
				LastUpdated: now,
			},
		})
	}

	return cameras, nil
}
