#!/bin/bash
# scripts/install/macos.sh
# One-command dependency setup for Pairion Client on macOS (ARM64 and x86_64)
# Follows CONVENTIONS.md and Phase B2 requirements. Idempotent.

set -e

echo "=== Pairion Client macOS Dependency Installer ==="

if ! command -v brew &> /dev/null; then
    echo "Homebrew not found. Installing..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
fi

echo "Updating Homebrew..."
brew update

PACKAGES=(
    cmake
    ninja
    clang-format
    opus
    onnxruntime
    qt@6
)

echo "Installing packages: ${PACKAGES[*]}"
brew install "${PACKAGES[@]}"

# Link Qt if needed
brew link --force qt@6 || true

echo "✅ macOS dependencies installed."
echo "Next: cmake --preset macos-arm64-debug (or macos-x86_64-debug)"
echo "For native tests: -DPAIRION_CLIENT_NATIVE_TESTS=ON"