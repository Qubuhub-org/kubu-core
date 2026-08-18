# 1. Create project directories
mkdir -p kubu-portal/dist/assets/js kubu-portal/dist/assets/css kubu-portal/www/kubucoin.org

cd kubu-portal

# 2. Create the files (you can paste the code from above into each)
touch compiler.js
touch server.go
touch build.sh
touch assets/js/wallet-engine.js
touch www/kubucoin.org/assets/css/main.css
touch www/kubucoin.org/html.yaml

# 3. Make the build script executable
chmod +x build.sh
