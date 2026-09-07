package main

import (
	"context"
	"fmt"
	"log"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/xanstomper/wheredaflock/backend/internal/handlers"
	"github.com/xanstomper/wheredaflock/backend/internal/store"
)

func main() {
	port := os.Getenv("PORT")
	if port == "" {
		port = "8080"
	}

	seedPath := os.Getenv("SEED_PATH")
	if seedPath == "" {
		candidates := []string{
			"data/cameras.json",
			"Backend/data/cameras.json",
			"../data/cameras.json",
			"WhereDaFlock/Resources/cameras.json",
		}
		for _, c := range candidates {
			if _, err := os.Stat(c); err == nil {
				seedPath = c
				break
			}
		}
	}

	memStore, err := store.NewMemoryStore(seedPath)
	if err != nil {
		log.Fatalf("Failed to initialize spatial store: %v", err)
	}

	apiHandler := handlers.NewAPIHandler(memStore)
	mux := http.NewServeMux()
	apiHandler.RegisterRoutes(mux)

	// Logging and CORS Middleware
	handler := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Access-Control-Allow-Origin", "*")
		w.Header().Set("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
		w.Header().Set("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With")

		if r.Method == http.MethodOptions {
			w.WriteHeader(http.StatusNoContent)
			return
		}

		start := time.Now()
		mux.ServeHTTP(w, r)
		log.Printf("[%s] %s %s - %v", r.Method, r.URL.Path, r.RemoteAddr, time.Since(start))
	})

	server := &http.Server{
		Addr:         ":" + port,
		Handler:      handler,
		ReadTimeout:  15 * time.Second,
		WriteTimeout: 15 * time.Second,
		IdleTimeout:  60 * time.Second,
	}

	stopChan := make(chan os.Signal, 1)
	signal.Notify(stopChan, os.Interrupt, syscall.SIGTERM)

	go func() {
		fmt.Printf(`
╔══════════════════════════════════════════════════════════════════╗
║               WhereDaFlock - Spatial Intelligence API            ║
╠══════════════════════════════════════════════════════════════════╣
║  • Port:           %-45s ║
║  • Cameras Loaded: %-45d ║
║  • Seed Source:    %-45s ║
╚══════════════════════════════════════════════════════════════════╝
`, ":"+port, memStore.TotalCameras(), seedPath)

		if err := server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			log.Fatalf("Server error: %v", err)
		}
	}()

	<-stopChan
	log.Println("Shutting down WhereDaFlock backend gracefully...")

	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	if err := server.Shutdown(ctx); err != nil {
		log.Fatalf("Server forced to shutdown: %v", err)
	}

	log.Println("WhereDaFlock backend exited safely.")
}
