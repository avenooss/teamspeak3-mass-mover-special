#!/usr/bin/env bash

# Build TS3MassMover for Linux and, when MinGW is available, Windows.

set -euo pipefail

SDK_DIR="ts3client-pluginsdk-26"

if [[ ! -d "$SDK_DIR/include" ]]; then
    echo "ERROR: TeamSpeak Plugin SDK v26 was not found in $SDK_DIR." >&2
    echo "Run ./setup_sdk.sh first." >&2
    exit 1
fi

mkdir -p build/linux build/windows bin/linux bin/windows

echo "Building Linux plugin..."
gcc -c -O2 -Wall -fPIC -std=gnu99 -I"$SDK_DIR/include" -Icjson src/massmover.c -o build/linux/massmover.o
gcc -c -O2 -Wall -fPIC -std=gnu99 -Icjson cjson/cJSON.c -o build/linux/cJSON.o
gcc -shared -o bin/linux/massmover.so build/linux/massmover.o build/linux/cJSON.o
echo "Linux build complete: bin/linux/massmover.so"

if command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then
    echo "Building 64-bit Windows plugin..."
    x86_64-w64-mingw32-gcc -c -O2 -Wall -DWIN32 -I"$SDK_DIR/include" -Icjson src/massmover.c -o build/windows/massmover.o
    x86_64-w64-mingw32-gcc -c -O2 -Wall -Icjson cjson/cJSON.c -o build/windows/cJSON.o
    x86_64-w64-mingw32-gcc -shared -o bin/windows/massmover.dll build/windows/massmover.o build/windows/cJSON.o
    echo "Windows build complete: bin/windows/massmover.dll"
elif command -v i686-w64-mingw32-gcc >/dev/null 2>&1; then
    echo "Building 32-bit Windows plugin..."
    i686-w64-mingw32-gcc -c -O2 -Wall -DWIN32 -I"$SDK_DIR/include" -Icjson src/massmover.c -o build/windows/massmover.o
    i686-w64-mingw32-gcc -c -O2 -Wall -Icjson cjson/cJSON.c -o build/windows/cJSON.o
    i686-w64-mingw32-gcc -shared -o bin/windows/massmover.dll build/windows/massmover.o build/windows/cJSON.o
    echo "Windows build complete: bin/windows/massmover.dll"
else
    echo "Windows cross-build skipped: MinGW-w64 is not installed."
fi

echo "Build finished. Generated files are under bin/."
echo "Installation is intentionally separate; see README.md."
