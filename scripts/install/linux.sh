#!/bin/bash
# scripts/install/linux.sh
# One-command dependency setup for Pairion Client on Linux (x86_64 and ARM64)
# Supports Ubuntu 24.04+ and similar. Follows Phase B2.

set -e

echo "=== Pairion Client Linux Dependency Installer ==="

if [ -f /etc/debian_version ]; then
    echo "Debian/Ubuntu detected. Installing via apt..."
    sudo apt-get update
    sudo apt-get install -y \
        build-essential cmake ninja-build clang-format \
        qt6-base-dev qt6-declarative-dev qt6-websockets-dev qt6-multimedia-dev \
        libopus-dev lcov \
        curl unzip
    
    # ONNX Runtime from upstream (as per plan; apt package may not be sufficient)
    if [ ! -d "/usr/local/lib/onnxruntime" ]; then
        echo "Installing ONNX Runtime from upstream..."
        ONNX_VERSION="1.18.0"
        ARCH=$(uname -m)
        if [ "$ARCH" = "aarch64" ]; then
            ONNX_URL="https://github.com/microsoft/onnxruntime/releases/download/v${ONNX_VERSION}/onnxruntime-linux-aarch64-${ONNX_VERSION}.tgz"
        else
            ONNX_URL="https://github.com/microsoft/onnxruntime/releases/download/v${ONNX_VERSION}/onnxruntime-linux-x64-${ONNX_VERSION}.tgz"
        fi
        curl -L -o onnxruntime.tgz "$ONNX_URL"
        sudo tar -xzf onnxruntime.tgz -C /usr/local --strip-components=1
        rm onnxruntime.tgz
        sudo ldconfig
        echo "ONNX Runtime installed to /usr/local"
    fi
elif command -v dnf &> /dev/null; then
    echo "Fedora/RHEL detected. Installing via dnf..."
    sudo dnf install -y cmake ninja-build clang-tools-extra qt6-qtbase-devel qt6-qtdeclarative-devel \
        qt6-qtwebsockets-devel qt6-qtmultimedia-devel opus-devel
    echo "Note: ONNX Runtime may require manual install from Microsoft releases."
else
    echo "Unsupported Linux distro. Please install Qt6, libopus, onnxruntime manually."
    exit 1
fi

echo "✅ Linux dependencies installed."
echo "Next: cmake --preset linux-x86_64-debug (or linux-arm64-debug)"
echo "For native tests: -DPAIRION_CLIENT_NATIVE_TESTS=ON"