#!/usr/bin/env bash

# Download the official TeamSpeak 3 Client Plugin SDK used by this project.

set -euo pipefail

SDK_VERSION="26"
SDK_DIR="ts3client-pluginsdk-${SDK_VERSION}"
SDK_URL="https://github.com/teamspeak/ts3client-pluginsdk/archive/refs/tags/${SDK_VERSION}.tar.gz"
SDK_ARCHIVE="${SDK_DIR}.tar.gz"

if [[ -d "$SDK_DIR/include" ]]; then
    echo "TeamSpeak Plugin SDK v${SDK_VERSION} is already available in $SDK_DIR."
    exit 0
fi

command -v tar >/dev/null 2>&1 || {
    echo "ERROR: tar is required." >&2
    exit 1
}

cleanup() {
    rm -f "$SDK_ARCHIVE"
}
trap cleanup EXIT

echo "Downloading TeamSpeak Plugin SDK v${SDK_VERSION}..."
if command -v curl >/dev/null 2>&1; then
    curl --fail --location --retry 3 --output "$SDK_ARCHIVE" "$SDK_URL"
elif command -v wget >/dev/null 2>&1; then
    wget --tries=3 --output-document="$SDK_ARCHIVE" "$SDK_URL"
else
    echo "ERROR: curl or wget is required." >&2
    exit 1
fi

echo "Extracting SDK..."
tar -xzf "$SDK_ARCHIVE"

if [[ ! -d "$SDK_DIR/include" ]]; then
    echo "ERROR: The downloaded archive did not contain the expected SDK directory." >&2
    exit 1
fi

echo "SDK setup complete: $SDK_DIR"
