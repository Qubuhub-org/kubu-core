package main

import (
	"bytes"
	"io"
	"log"
	"net/http"
	"time"
)

const (
	StaticDir  = "./dist"
	KubuRpcUrl = "http://127.0.0.1:33873" // Default Kubu Core mainnet RPC port
	ServerPort = ":8080"
)

func main() {
	mux := http.NewServeMux()

	// 1. Static File Server for Compiled Web4 App
	fileServer := http.FileServer(http.Dir(StaticDir))
	mux.Handle("/", fileServer)

	// 2. Safe RPC Proxy Endpoint
	mux.HandleFunc("/api/rpc", func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodPost {
			http.Error(w, "Method not allowed", http.StatusMethodNotAllowed)
			return
		}

		body, err := io.ReadAll(r.Body)
		if err != nil {
			http.Error(w, "Bad request", http.StatusBadRequest)
			return
		}

		// Forward RPC query to local kubu-core daemon
		req, err := http.NewRequest(http.MethodPost, KubuRpcUrl, bytes.NewBuffer(body))
		if err != nil {
			http.Error(w, "Internal server error", http.StatusInternalServerError)
			return
		}
		req.Header.Set("Content-Type", "application/json")

		client := &http.Client{Timeout: 10 * time.Second}
		resp, err := client.Do(req)
		if err != nil {
			http.Error(w, "Kubu Core node unreachable", http.StatusBadGateway)
			return
		}
		defer resp.Body.Close()

		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(resp.StatusCode)
		io.Copy(w, resp.Body)
	})

	log.Printf("[Kubu Portal] Server listening on http://localhost%s\n", ServerPort)
	if err := http.ListenAndServe(ServerPort, mux); err != nil {
		log.Fatalf("Server failed: %v", err)
	}
}
