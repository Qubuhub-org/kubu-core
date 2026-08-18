# Makefile for Kubu Core & Web Portal

.PHONY: build run test clean

# Compile the Go daemon & Web compiler
build:
	@echo "Building kubu portal..."
	@node compiler.js
	@go build -o bin/kubu-portal server.go

# Start the node & server in background
run: build
	@echo "Starting Kubu Portal..."
	@./bin/kubu-portal &

# Check node health via kubu-cli
status:
	@kubu-cli getblockchaininfo

clean:
	@rm -rf dist/ bin/
