-- WhereDaFlock PostGIS Spatial Schema
-- Extension enablement
CREATE EXTENSION IF NOT EXISTS postgis;

-- Surveillance cameras table
CREATE TABLE IF NOT EXISTS cameras (
    id TEXT PRIMARY KEY,
    type TEXT NOT NULL,
    geom GEOMETRY(Point, 4326) NOT NULL,
    address TEXT,
    owner TEXT,
    confidence REAL DEFAULT 95.0,
    is_confirmed BOOLEAN DEFAULT true,
    source_name TEXT,
    source_reliability REAL,
    last_verified TIMESTAMPTZ DEFAULT NOW(),
    created_at TIMESTAMPTZ DEFAULT NOW()
);

-- Ephemeral community hazard and camera reports
CREATE TABLE IF NOT EXISTS reports (
    id TEXT PRIMARY KEY,
    type TEXT NOT NULL,
    geom GEOMETRY(Point, 4326) NOT NULL,
    description TEXT,
    confidence REAL DEFAULT 50.0,
    upvotes INT DEFAULT 0,
    downvotes INT DEFAULT 0,
    is_verified BOOLEAN DEFAULT false,
    is_anonymous BOOLEAN DEFAULT true,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    expires_at TIMESTAMPTZ NOT NULL
);

-- High-performance GiST spatial bounding indexes
CREATE INDEX IF NOT EXISTS idx_cameras_geom ON cameras USING GIST (geom);
CREATE INDEX IF NOT EXISTS idx_reports_geom ON reports USING GIST (geom);
CREATE INDEX IF NOT EXISTS idx_reports_expires ON reports (expires_at);
