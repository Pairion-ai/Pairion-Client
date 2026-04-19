#!/usr/bin/env bash
# scripts/install/linux-fedora.sh — Idempotent Fedora/RHEL dependency installer for Pairion-Client.
# Tested on Fedora 40. Requires sudo.

set -euo pipefail

echo "=== Pairion Client — Fedora/RHEL Dependency Installer ==="

sudo dnf install -y \
    cmake ninja-build clang-tools-extra \
    qt6-qtbase-devel qt6-qtdeclarative-devel \
    qt6-qtwebsockets-devel qt6-qtmultimedia-devel \
    opus-devel lcov curl

# ONNX Runtime — not available in standard Fedora repos; install from upstream.
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
