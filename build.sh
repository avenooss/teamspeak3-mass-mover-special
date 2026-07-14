#!/bin/bash

# TeamSpeak 3 MassMover Plugin Build Script
# Builds for Linux and Windows (using MinGW)

set -e

echo "Building TeamSpeak 3 MassMover Plugin..."

# Create directories
mkdir -p build/linux build/windows bin/linux bin/windows

# Build for Linux
echo "Building for Linux..."
gcc -c -g -O0 -Wall -fPIC -std=gnu99 -Its3client-pluginsdk-26/include -Icjson src/massmover.c -o build/linux/massmover.o
gcc -c -g -O0 -Wall -fPIC -std=gnu99 -Icjson cjson/cJSON.c -o build/linux/cJSON.o
gcc -shared -o bin/linux/massmover.so build/linux/massmover.o build/linux/cJSON.o
echo "✓ Linux build complete: bin/linux/massmover.so"


# Copy to Flatpak location
echo "Copying to Flatpak TeamSpeak 3 plugins directory..."
cp bin/linux/massmover.so ~/.var/app/com.teamspeak.TeamSpeak3/.ts3client/plugins
echo "✓ Plugin copied to Flatpak location"

# Build for Windows (using MinGW cross-compiler if available)
if command -v x86_64-w64-mingw32-gcc &> /dev/null; then
    echo "Building for Windows (MinGW cross-compiler)..."
    x86_64-w64-mingw32-gcc -c -O2 -Wall -DWIN32 -Its3client-pluginsdk-26/include -Icjson src/massmover.c -o build/windows/massmover.o
    x86_64-w64-mingw32-gcc -c -O2 -Wall -Icjson cjson/cJSON.c -o build/windows/cJSON.o
    x86_64-w64-mingw32-gcc -shared -o bin/windows/massmover.dll build/windows/massmover.o build/windows/cJSON.o
    echo "✓ Windows build complete: bin/windows/massmover.dll"
elif command -v i686-w64-mingw32-gcc &> /dev/null; then
    echo "Building for Windows (32-bit MinGW cross-compiler)..."
    i686-w64-mingw32-gcc -c -O2 -Wall -DWIN32 -Its3client-pluginsdk-26/include -Icjson src/massmover.c -o build/windows/massmover.o
    i686-w64-mingw32-gcc -c -O2 -Wall -Icjson cjson/cJSON.c -o build/windows/cJSON.o
    i686-w64-mingw32-gcc -shared -o bin/windows/massmover.dll build/windows/massmover.o build/windows/cJSON.o
    echo "✓ Windows build complete: bin/windows/massmover.dll"
else
    echo "⚠️  Windows cross-compiler not found. Install mingw-w64 to build for Windows:"
    echo "   Ubuntu/Debian: sudo apt-get install mingw-w64"
    echo "   Fedora: sudo dnf install mingw64-gcc"
    echo "   macOS: brew install mingw-w64"
fi

echo ""
echo "Build complete!"
echo ""
echo "Installation:"
echo "  Linux (Flatpak): Plugin has been automatically installed to Flatpak TeamSpeak 3"
echo "  Linux (Native):  Copy bin/linux/massmover.so to ~/.ts3client/plugins/"
echo "  Windows:        Copy bin/windows/massmover.dll to %APPDATA%\\TS3Client\\plugins\\"
echo ""
echo "Then restart TeamSpeak and enable the plugin in Settings > Plugins"
