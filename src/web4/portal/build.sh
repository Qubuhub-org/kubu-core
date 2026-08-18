#!/usr/bin/env bash
set -e

echo "=== Building Kubucoin Web4 Portal Stack ==="

# 1. Setup build directories
mkdir -p dist/assets/js dist/assets/css

# 2. Install Dependencies
if [ ! -d "node_modules" ]; then
  npm install js-yaml
fi

# 3. Compile Web4 Manifest to HTML
node compiler.js

# 4. Copy Static Assets
cp www/kubucoin.org/assets/css/main.css dist/assets/css/
cp assets/js/wallet-engine.js dist/assets/js/

# 5. Build Proxy Web Server
go build -o kubu-portal server.go

echo "=== Build Complete ==="
echo "Run './kubu-portal' to launch server on http://localhost:8080"
