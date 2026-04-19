# Pairion Client

Native Qt 6 desktop client for the Pairion household AI assistant. Connects to Pairion-Server via WebSocket, displays a debug panel showing connection state, and will evolve into the full cinematic HUD.

## Prerequisites & One-Command Install (Phase B2)

Run the appropriate script from `scripts/install/` (creates all deps, idempotent):

```bash
# macOS (ARM64 or Intel)
./scripts/install/macos.sh

# Linux (Ubuntu 24.04+ recommended)
./scripts/install/linux.sh

# Windows (PowerShell as Administrator)
./scripts/install/windows.ps1
```

**Supported targets (new presets):**
- macOS arm64, x86_64
- Linux x86_64, arm64
- Windows x86_64

Requires: CMake 3.25+, Ninja, Qt 6.6+ (installed by script), libopus, onnxruntime (vcpkg/tar.gz/Homebrew).

## Microphone Permission

On macOS, the client requests microphone access at launch via the QPermission API. If permission is denied, the debug panel shows an error message and voice interaction is unavailable — but the WebSocket connection and debug panel remain functional.

## First-Run Model Download

On first launch after connecting to the server, the client downloads ONNX models for wake word detection (openWakeWord) and voice activity detection (Silero VAD) to `~/Library/Application Support/Pairion/models/` (macOS). Models are SHA-256 verified and cached for subsequent launches.

## Native Model Tests

To enable tests that exercise real ONNX model files (requires models to be downloaded):

```bash
cmake --preset macos-debug -DPAIRION_CLIENT_NATIVE_TESTS=ON
```

## Build (use platform-specific preset)

```bash
# Example: macOS ARM64 debug (recommended for M4)
cmake --preset macos-arm64-debug
cmake --build --preset macos-arm64-debug

# Other examples:
# cmake --preset macos-x86_64-debug
# cmake --preset linux-x86_64-debug
# cmake --preset windows-x86_64-debug
```

## Run

```bash
./build/macos-debug/pairion.app/Contents/MacOS/pairion
```

The debug panel window will open showing connection state. If Pairion-Server is running on `ws://localhost:18789/ws/v1`, the client will connect, identify, and display the session ID and server version.

## Test

```bash
ctest --preset macos-debug --output-on-failure
```

## Coverage

```bash
# Requires lcov and genhtml — enforces 100% line coverage on src/
cmake --build --preset macos-debug --target coverage
open build/macos-debug/coverage_report/index.html
```

## Format Check

```bash
find src test -name '*.cpp' -o -name '*.h' | xargs clang-format --dry-run --Werror
```

## Project Structure

```
src/
  core/       — Constants, device identity, shared types
  protocol/   — WebSocket message types and JSON codec
  ws/         — QWebSocket client with heartbeat and reconnect
  state/      — QML-exposed singleton state objects
  util/       — Centralized logging with batched forwarding
  audio/      — (M1) Audio capture and playback
  wake/       — (M1) Wake-word detection
  vad/        — (M1) Voice activity detection
  pipeline/   — (M1) Audio session orchestrator
  hud/        — (M2) Cinematic HUD components
  settings/   — (future) Settings UI and persistence
qml/
  Main.qml         — Root application window
  Debug/            — M0 debug panel components
test/
  tst_message_codec.cpp          — Protocol codec round-trip tests
  tst_ws_client_state_machine.cpp — WebSocket state machine tests
  tst_log_batching.cpp           — Logger batching tests
  tst_integration.cpp            — Full integration tests with mock server
  mock_server.h/cpp              — Mock WebSocket server harness
```
