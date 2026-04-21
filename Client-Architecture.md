# Pairion-Client Architecture

**Version:** 2.2
**Status:** Foundational
**Stack:** Qt 6.6+ + C++ 20 + QML (Qt Quick)

---

## 1. Purpose

The Client is the household's face for Pairion. Native Qt binaries for macOS, Windows, and Linux. Cinematic HUD, microphone capture, wake-word detection, audio playback, state display. Thin — all intelligent work happens on the Server.

A future Pairion-Mobile project will be a separate Flutter codebase targeting iOS and Android, speaking the same WebSocket protocol.

## 2. Top-Level Shape

```
Pairion-Client/  (Qt 6 CMake project)
├── CMakeLists.txt
├── conanfile.txt or vcpkg.json (dependency manifest)
├── src/
│   ├── main.cpp                    (entry point)
│   ├── core/                       (shared types, constants, utilities)
│   ├── protocol/                   (WebSocket message types generated from asyncapi.yaml; envelope codec)
│   ├── audio/                      (QAudioSource/QAudioSink wrappers, Opus encode/decode via libopus or FFmpeg)
│   ├── wake/                       (openWakeWord ONNX pipeline via onnxruntime C++ API)
│   ├── vad/                        (Silero VAD ONNX)
│   ├── ws/                         (QWebSocket client, reconnect, heartbeat, binary routing)
│   ├── pipeline/                   (AudioSessionOrchestrator)
│   ├── hud/                        (QML + C++ backends for the cinematic surface)
│   ├── state/                      (singleton state objects exposed to QML)
│   ├── settings/                   (settings persistence)
│   └── util/                       (logging, error handling)
├── qml/
│   ├── Main.qml                    (root window)
│   ├── HUD/                        (HUD visual components)
│   ├── Debug/                      (M0 debug panel)
│   └── Settings/
├── shaders/                        (.vert / .frag pre-compiled to .qsb via qsb tool)
├── resources/
│   ├── models/                     (ONNX files — downloaded on first run, cached)
│   └── sounds/                     (wake chime, completion sigh — M2)
├── resources.qrc                   (Qt resource bundle manifest)
├── test/                           (Qt Test framework unit + integration tests)
├── platform/
│   ├── macos/                      (Info.plist, entitlements)
│   ├── windows/                    (installer config)
│   └── linux/                      (.desktop file, AppImage config)
├── Architecture.md                 (this file)
├── CONVENTIONS.md
├── README.md
└── CHANGELOG.md
```

## 3. Build System

- **CMake** primary build system (Qt 6 standard)
- **Conan** or **vcpkg** for third-party C++ dependencies (onnxruntime, libopus). Qt itself installed via online installer or `aqtinstall`; not managed by the package manager.
- Cross-platform CMake presets: `macos-debug`, `macos-release`, `windows-debug`, `windows-release`, `linux-debug`, `linux-release`
- `CMakeLists.txt` uses `find_package(Qt6 COMPONENTS Core Quick Multimedia WebSockets Network Test)` pattern

## 4. Core Modules (C++)

- `core` — Type aliases, constants, error types, utility classes. No Qt dependencies where avoidable.
- `protocol` — Generated or hand-written message classes matching every payload in `asyncapi.yaml`. Serialization via `QJsonDocument`/`QJsonObject` for text frames; binary frames are raw `QByteArray` with a 4-byte stream ID prefix.
- `audio` — `PairionAudioCapture` (wraps `QAudioSource` at 16 kHz mono PCM, pushes to Opus encoder), `PairionAudioPlayback` (wraps `QAudioSink`, consumes decoded PCM from jitter buffer), `PairionOpusEncoder` / `PairionOpusDecoder` (via `libopus`).
- `wake` — `OpenWakeWordDetector` (three-model ONNX pipeline via `onnxruntime` C++ API: melspectrogram → embedding → hey_jarvis).
- `vad` — `SileroVad` (single-model ONNX).
- `ws` — `PairionWebSocketClient` wrapping `QWebSocket`. Manages connect, identify, heartbeat, reconnect (exponential backoff), inbound demux (text JSON to router, binary to orchestrator).
- `pipeline` — `AudioSessionOrchestrator` (QObject-based, signals/slots). Coordinates: wake fire → stream start → encoder → WS send; inbound binary → decoder → jitter → playback; state emission for HUD.
- `hud` — C++ classes exposed to QML via `Q_PROPERTY` / `qmlRegisterType`. Scene-graph custom items for frame-perfect rendering.
- `state` — Singleton QObjects that QML binds to. `ConnectionState`, `SessionState`, `HudState`, `SettingsState`.
- `settings` — Uses `QSettings` for persistence including the device bearer token. QtKeychain was evaluated and removed during M0 due to async-callback incompatibility with GUI-less Qt Test contexts. Acceptable trade-off during development (per CONVENTIONS §0 preference for minimal development-phase friction); production hardening is an M12 launch-polish concern.
- `util` — Logging wrapper around `QLoggingCategory` + custom message handler that forwards to Server via POST `/v1/logs`.

## 5. QML / Qt Quick Structure

- `Main.qml` — `ApplicationWindow` root, full-screen on macOS (or configurable). Loads either the debug panel (M0) or the HUD (M2+) based on state.
- HUD components (M2) use `Item`, `Rectangle`, `ShaderEffect`, `MultiEffect`, custom `QSGRenderNode`-based C++ items where shader effects don't suffice.
- Shaders compiled offline via the `qsb` tool and shipped in `resources.qrc`.
- Animation via `Behavior`, `NumberAnimation`, `SequentialAnimation` for smooth state transitions.

## 6. Concurrency Model

- **Main thread:** QML, scene graph, widget / UI events.
- **Scene graph render thread:** owned by Qt; renders offscreen.
- **Audio capture thread:** `QAudioSource` runs on a dedicated thread; uses `moveToThread` pattern.
- **Audio playback thread:** same pattern for `QAudioSink`.
- **Encode / decode workers:** Opus encoding and decoding run on worker threads via `QThreadPool`, communicating via thread-safe queues.
- **ONNX inference:** wake and VAD models run on their own worker thread to avoid blocking capture.
- **WebSocket:** `QWebSocket` runs on the main thread's event loop (Qt's signals/slots handle cross-thread delivery safely).

Communication between threads uses Qt signals/slots with `Qt::QueuedConnection` or `Qt::AutoConnection` — Qt's built-in thread-safety mechanism.

## 7. Audio Pipeline

### 7.1 Capture
- `QAudioSource` opened with `QAudioFormat` at 16 kHz, mono, signed 16-bit
- PCM samples pushed into a lock-free ring buffer
- Wake-word detector consumes from ring buffer continuously
- On wake fire: ring buffer contents (pre-wake ~200 ms) plus new samples streamed to Opus encoder
- Opus encoder produces 20 ms frames at 16 kHz, 24-32 kbps VBR
- Encoded frames packaged as `AudioChunkIn` binary messages and queued to WS sender

### 7.2 Playback
- `QAudioSink` opened with `QAudioFormat` at the codec's sample rate
- Inbound `AudioChunkOut` binary frames routed to Opus decoder
- Decoded PCM fed into jitter buffer (40-60 ms target)
- `QAudioSink`'s pull mode drains the buffer at output-device rate
- On `AudioStreamEnd { reason: normal }`: drain cleanly
- On `reason: interrupted`: flush buffer immediately (barge-in scaffold; full behavior M4)

## 8. Wake Word and VAD

Both use `onnxruntime` C++ API (add to `CMakeLists.txt` via Conan or vcpkg).

**openWakeWord pipeline** (three ONNX models in sequence):
- `melspectrogram.onnx` → `embedding_model.onnx` → `hey_jarvis_v0.1.onnx`
- All from `github.com/dscripka/openWakeWord/releases/download/v0.5.1/`
- Downloaded on first run, SHA-256 verified, cached in user-specific app data directory (`QStandardPaths::AppDataLocation`)
- Threshold 0.5 default
- 500 ms false-wake suppression

**Silero VAD:**
- `silero_vad.onnx` (same openWakeWord release, verified mirror)
- Threshold 0.5 default
- 800 ms sustained silence triggers end-of-speech

Model license: CC BY-NC-SA 4.0 — compatible with Pairion's own posture per Charter §13.3.

## 9. WebSocket Client

`PairionWebSocketClient` uses `QWebSocket`:

- Connects to `ws://localhost:18789/ws/v1`
- On connection: sends `DeviceIdentify` with bearer token
- On `SessionOpened`: emits signal with session ID, server version
- Heartbeat loop runs continuously (QTimer at 15 s interval)
- Handles disconnection: emits signal, starts reconnection with exponential backoff (1 s → 2 s → 4 s → 8 s → 15 s → 30 s cap)
- Demultiplexes inbound frames: text frames parsed as JSON envelopes → dispatched by `type` discriminator; binary frames → extract 4-byte stream ID → route to orchestrator

## 10. Audio Session Orchestrator

`AudioSessionOrchestrator` is a QObject with signal/slot command interface:

**Slots (incoming commands):**
- `startFromWake(QString streamId, QByteArray preRollBuffer)`
- `startManual(QString streamId)`
- `stopCurrent()`
- `inboundAudio(QString streamId, QByteArray opusFrame)`
- `inboundStreamEnd(QString streamId, QString reason)`
- `shutdown()`

**Signals (outgoing events):**
- `agentStateChanged(AgentState state)` — feeds HUD
- `pipelineError(QString message)` — feeds log

## 11. State Exposure to QML

C++ state singletons registered with QML via `qmlRegisterSingletonType`. Widgets use property bindings:

```qml
// Illustrative (no code in prompts)
// Access pattern: ConnectionState.status, SessionState.sessionId, HudState.voiceState
```

## 12. Platform Considerations

- **macOS:** Primary target. `Info.plist` needs `NSMicrophoneUsageDescription`. App sandbox entitlements for network and audio IO.
- **Windows:** Installer via NSIS or Qt Installer Framework. Microphone permission handled by Windows 10/11's permission system.
- **Linux:** AppImage or Flatpak for distribution. PulseAudio / PipeWire audio backend handled by Qt Multimedia.

## 13. Logging

- Custom message handler installed via `qInstallMessageHandler`
- Routes `qDebug`, `qInfo`, `qWarning`, `qCritical` through structured JSON formatter
- Log records batched in memory, forwarded to Server via POST `/v1/logs` every 30 s
- Categories via `QLoggingCategory` for per-module control

## 14. Testing

- **Qt Test framework** for unit tests
- Custom mock WebSocket server for integration tests (tests the full pipeline against a deterministic protocol emitter)
- 100% line coverage required on all modules in `src/` (measured via `gcov` or Clang source-based coverage)
- Widget / QML tests via `QtQuickTest`
- Shader tests ensure all shipped `.qsb` files compile for every target graphics API (Metal, Vulkan, D3D11)

## 15. Secrets

- Device bearer token stored via `QSettings` (standard Qt persistence; cross-platform ini/plist/registry).
- QtKeychain was evaluated during M0 and removed due to async-callback delivery not working inside `QTEST_GUILESS_MAIN` test contexts, which blocked verification. QSettings is the development-phase choice.
- **Not production-ready for commercial deployment.** Production hardening of secret storage is an M12 launch-polish concern. Acceptable approaches to revisit then: restoring QtKeychain with a synchronous wrapper and abstracted `SecretStore` interface for testability; OS-native secret APIs called directly; or a dedicated encrypted keystore.
- No API keys on the Client. All third-party calls go through the Server.

## 16. DevTools and Diagnostics

- Qt Creator's QML Profiler for HUD performance work
- `QSG_RENDER_TIMING=1` environment variable for scene-graph timing output
- Built-in debug panel (M0 UI) shows connection state, session ID, server version, last 10 log records, reconnection attempts. Usable without Qt Creator.