# Multi-stage build for WhereDaFlock Go Backend
FROM golang:1.22-alpine AS builder

WORKDIR /app

# Install build dependencies
RUN apk add --no-cache git ca-certificates

# Copy go modules
COPY go.mod ./
RUN go mod download || true

# Copy source code
COPY . .

# Build statically compiled binary
RUN CGO_ENABLED=0 GOOS=linux go build -ldflags="-s -w" -o /app/bin/server ./cmd/server

# Final lightweight runner image
FROM alpine:3.19

RUN apk --no-cache add ca-certificates tzdata

WORKDIR /app

# Copy binary from builder
COPY --from=builder /app/bin/server /app/server
COPY data/ /app/data/

EXPOSE 8080

ENV PORT=8080
ENV SEED_PATH=/app/data/cameras.json

ENTRYPOINT ["/app/server"]
