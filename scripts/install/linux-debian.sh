#!/usr/bin/env bash
# scripts/install/linux-debian.sh — Idempotent Debian/Ubuntu dependency installer for Pairion-Client.
# Tested on Ubuntu 24.04 LTS. Requires sudo.

set -euo pipefail

echo "=== Pairion Client — Debian/Ubuntu Dependency Installer ==="

sudo apt-get update
sudo apt-get install -y \
    build-essential cmake ninja-build clang-format \
    qt6-base-dev qt6-declarative-dev qt6-websockets-dev qt6-multimedia-dev \
    libopus-dev lcov curl unzip

# ONNX Runtime — install from Microsoft upstream releases if not already present.
if pkg-config --exists libonnxruntime 2>/dev/null || [ -f /usr/local/lib/cmake/onnxruntime/onnxruntimeConfig.cmake ]; then
    echo "  onnxruntime already installed"
else
    echo "  Installing ONNX Runtime from upstream GitHub releases..."
    ONNX_VERSION="1.18.0"
    ARCH=$(uname -m)
    if [ "$ARCH" = "aarch64" ]; then
        ONNX_TAR="onnxruntime-linux-aarch64-${ONNX_VERSION}.tgz"
    else
        ONNX_TAR="onnxruntime-linux-x64-${ONNX_VERSION}.tgz"
    fi
    ONNX_URL="https://github.com/microsoft/onnxruntime/releases/download/v${ONNX_VERSION}/${ONNX_TAR}"
    curl -fsSL -o "/tmp/${ONNX_TAR}" "$ONNX_URL"
    sudo tar -xzf "/tmp/${ONNX_TAR}" -C /usr/local --strip-components=1
    rm "/tmp/${ONNX_TAR}"
    sudo ldconfig
    echo "  onnxruntime installed to /usr/local"
fi

echo ""
echo "=== Done ==="
echo "Build with:"
echo "  cmake --preset linux-$(uname -m | sed 's/aarch64/arm64/')-debug"
echo "  cmake --build --preset linux-$(uname -m | sed 's/aarch64/arm64/')-debug"
