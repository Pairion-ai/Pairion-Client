# Pairion Client

Native Qt 6 desktop client for the Pairion household AI assistant. Connects to Pairion-Server via WebSocket, displays a debug panel showing connection state, and will evolve into the full cinematic HUD.

## Prerequisites

- CMake 3.25+
- Ninja build system
- Qt 6.6+ with modules: Core, Quick, Network, WebSockets, Test
- clang-format (for code formatting checks)

### macOS

```bash
brew install qt cmake ninja clang-format
```

### Windows

Install Qt 6.6+ via the online installer. Use MSVC 2022 with CMake and Ninja.

### Linux

Install Qt 6.6+ via your distribution's package manager or `aqtinstall`. Install QtKeychain via your package manager or build from source.

```bash
# Ubuntu/Debian
sudo apt install qt6-base-dev qt6-declarative-dev qt6-websockets-dev \
    cmake ninja-build clang-format lcov
```

## Build

```bash
# Configure (macOS debug)
cmake --preset macos-debug

# Build
cmake --build --preset macos-debug
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
# Requires lcov and genhtml
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
