# 1. Create the folder in AppData\Roaming
New-Item -ItemType Directory -Force -Path "$env:APPDATA\Kubu"

# 2. Create and open the file in Notepad
notepad "$env:APPDATA\Kubu\kubu.conf"
