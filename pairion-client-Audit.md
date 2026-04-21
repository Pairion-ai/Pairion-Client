# Pairion-Client — Codebase Audit

**Audit Date:** 2026-04-21T22:38:18Z
**Branch:** main
**Commit:** 05b16cc4985bf7ff16ce7688cb6f8d360db40693 revert: remove conversation mode — VAD loopback incompatible without AEC (PC-CONV-revert)
**Auditor:** Claude Code (Automated)
**Purpose:** Zero-context reference for AI-assisted development
**Language:** C++20 / Qt6 / QML
**Build:** CMake + Ninja

> This audit is the source of truth for the Pairion-Client codebase structure, classes, modules, and configuration.
> An AI reading this audit should be able to generate accurate code changes, new features, tests, and fixes without filesystem access.

---
### Section 1 — Project Identity

| Field | Value |
|---|---|
| Project Name | Pairion Client |
| Repository | Pairion-Client |
| Language / Framework | C++20 / Qt6 / QML |
| C++ Standard | C++20 (enforced via `set(CMAKE_CXX_STANDARD 20)`) |
| CMake Version | 4.2.3 (minimum required: 3.25) |
| Qt Version | 6.11.0 (via Homebrew at `/opt/homebrew`) |
| Build System | CMake + Ninja |
| Project Version | 0.3.0 (from `project(pairion VERSION 0.3.0)`) |
| Branch | main |
| Latest Commit | 05b16cc4985bf7ff16ce7688cb6f8d360db40693 revert: remove conversation mode — VAD loopback incompatible without AEC (PC-CONV-revert) |
| Audit Timestamp | See header |

**Summary:** Desktop voice assistant client targeting macOS ARM64 (M1+) with cross-platform CMake presets. Uses Qt6 Quick/QML for the full-screen cinematic HUD and QWebSocket for server communication.

---
### Section 2 — Directory Structure

```
Pairion-Client/
├── CMakeLists.txt                           # Root build file; defines pairion_core + pairion exe + 16 test targets
├── CMakePresets.json                        # Platform presets: macOS ARM64/x86_64, Linux x86_64/ARM64, Windows x86_64
├── CLAUDE.md                                # Behavioral contract for Claude Code
├── CONVENTIONS.md                           # Coding conventions
├── Architecture.md                          # Architecture reference
├── CHANGELOG.md                             # Project changelog
├── README.md                                # Project readme
├── resources/
│   └── model_manifest.json                  # ONNX model download URLs + SHA-256 hashes
├── cmake/
│   ├── check_coverage.cmake                 # Enforces 100% line coverage via LCOV tracefile parsing
│   └── filter_coverage.cmake                # Filters LCOV data (removes system/Qt/generated files)
├── src/
│   ├── main.cpp                             # Application entry point
│   ├── core/
│   │   ├── constants.h                      # App-wide compile-time constants
│   │   ├── device_identity.h/.cpp           # UUID device ID + bearer token via QSettings
│   │   ├── model_downloader.h/.cpp          # ONNX model download + SHA-256 verification
│   │   ├── onnx_session.h/.cpp              # Abstract ONNX session + OrtInferenceSession impl
│   │   └── README.md
│   ├── audio/
│   │   ├── pairion_audio_capture.h/.cpp     # Mic capture at 16 kHz via QAudioSource
│   │   ├── pairion_audio_playback.h/.cpp    # QAudioSink playback with jitter buffer
│   │   ├── pairion_opus_encoder.h/.cpp      # libopus encoder (16 kHz mono → Opus)
│   │   ├── pairion_opus_decoder.h/.cpp      # libopus decoder (Opus → PCM)
│   │   ├── ring_buffer.h                    # Lock-free SPSC ring buffer (header-only template)
│   │   └── README.md
│   ├── pipeline/
│   │   ├── audio_session_orchestrator.h/.cpp # Central state machine (Idle→AwaitingWake→Streaming)
│   │   └── README.md
│   ├── protocol/
│   │   ├── messages.h                       # All WebSocket message structs + variant types
│   │   ├── envelope_codec.h/.cpp            # JSON text-frame serializer/deserializer
│   │   └── binary_codec.h/.cpp              # Binary frame encoder/decoder (4-byte stream prefix)
│   ├── state/
│   │   └── connection_state.h/.cpp          # QML-exposed singleton: connection + pipeline state
│   ├── settings/
│   │   ├── settings.h/.cpp                  # QSettings-backed runtime config singleton
│   │   └── README.md
│   ├── util/
│   │   └── logger.h/.cpp                    # Centralized logger: Qt msg handler + batched HTTP POST
│   ├── vad/
│   │   ├── silero_vad.h/.cpp                # Silero VAD v5 ONNX wrapper + state machine
│   │   └── README.md
│   ├── wake/
│   │   ├── open_wakeword_detector.h/.cpp    # openWakeWord 3-model ONNX pipeline
│   │   └── README.md
│   └── hud/
│       └── README.md
├── qml/
│   ├── Main.qml                             # Root ApplicationWindow; F12=debug/HUD toggle, F11=FPS
│   ├── HUD/
│   │   ├── PairionHUD.qml                   # Full-screen cinematic HUD root item
│   │   ├── HudLayout.qml                    # Panel layout (2560×1440 reference geometry)
│   │   ├── RingSystem.qml                   # Five concentric animated rings (state-driven)
│   │   ├── HemisphereMap.qml                # World map with globe scroll, pin focus, zoom
│   │   ├── TopBar.qml                       # World city clocks (Dallas/London/Tokyo)
│   │   ├── TranscriptStrip.qml              # Full-width STT transcript footer
│   │   ├── DashboardPanel.qml               # Reusable panel container (border + header)
│   │   ├── DashboardPanels.qml              # Four data panels: News, Inbox, Todo, Homestead
│   │   ├── ContextBackground.qml            # Context-driven background (earth/space)
│   │   └── FpsCounter.qml                   # FPS overlay (F11 toggle)
│   └── Debug/
│       └── DebugPanel.qml                   # M0/M1 debug panel showing all connection state
└── test/
    ├── mock_onnx_session.h                  # MockOnnxSession: canned ONNX outputs for tests
    ├── mock_server.h/.cpp                   # MockServer: QWebSocketServer for integration tests
    ├── tst_message_codec.cpp                # EnvelopeCodec serialize/deserialize tests
    ├── tst_log_batching.cpp                 # Logger batch/flush/callback tests
    ├── tst_ws_client_state_machine.cpp      # PairionWebSocketClient state machine tests
    ├── tst_device_identity.cpp              # DeviceIdentity creation/persistence tests
    ├── tst_integration.cpp                  # Full pipeline integration tests
    ├── tst_binary_frame.cpp                 # BinaryCodec encode/decode tests
    ├── tst_settings.cpp                     # Settings defaults/persistence/signals tests
    ├── tst_ring_buffer.cpp                  # RingBuffer push/pop/wrap tests
    ├── tst_opus_codec.cpp                   # Opus encode/decode round-trip tests
    ├── tst_wake_detector.cpp                # OpenWakewordDetector threshold/suppression tests
    ├── tst_silero_vad.cpp                   # SileroVad speech detection tests
    ├── tst_model_downloader.cpp             # ModelDownloader path/download tests
    ├── tst_audio_session_orchestrator.cpp   # AudioSessionOrchestrator state machine tests
    ├── tst_inbound_playback.cpp             # PairionAudioPlayback inbound audio tests
    ├── tst_constants.cpp                    # Constants sanity tests
    └── tst_connection_state.cpp             # ConnectionState map/background state tests
```

**Layout Summary:** Source is organized into 8 modules under `src/` (core, audio, pipeline, protocol, state, settings, util, vad, wake) plus a QML UI layer under `qml/` and a comprehensive test suite under `test/`. The flat module structure maps cleanly to the pipeline data flow: capture → wake/VAD → encode → WebSocket.

---
### Section 3 — Build & Dependency Manifest

#### Dependencies

| Dependency | Type | Version | Notes |
|---|---|---|---|
| Qt6::Core | Qt module | 6.11.0 | Required ≥ 6.5 |
| Qt6::Quick | Qt module | 6.11.0 | QML engine |
| Qt6::Network | Qt module | 6.11.0 | HTTP (model downloader, logger) |
| Qt6::WebSockets | Qt module | 6.11.0 | WebSocket client |
| Qt6::Multimedia | Qt module | 6.11.0 | QAudioSource, QAudioSink |
| Qt6::Test | Qt module | 6.11.0 | Test framework |
| opus | System lib | via pkg-config | libopus voice codec |
| onnxruntime | CMake package | system | ONNX model inference |

#### Build Targets

| Target | Type | Purpose |
|---|---|---|
| `pairion_core` | STATIC library | All src/ except main.cpp — testable unit |
| `pairion` | Executable | macOS bundle; links pairion_core |
| `tst_message_codec` | Test exe | M0 protocol message codec |
| `tst_log_batching` | Test exe | M0 logger batching |
| `tst_ws_client_state_machine` | Test exe | M0 WebSocket state machine |
| `tst_device_identity` | Test exe | M0 device identity |
| `tst_integration` | Test exe | M0 integration |
| `tst_binary_frame` | Test exe | M1 binary codec |
| `tst_settings` | Test exe | M1 settings + audio capture |
| `tst_ring_buffer` | Test exe | M1 ring buffer |
| `tst_opus_codec` | Test exe | M1 Opus encode/decode |
| `tst_wake_detector` | Test exe | M1 wake word |
| `tst_silero_vad` | Test exe | M1 VAD |
| `tst_model_downloader` | Test exe | M1 model downloader |
| `tst_audio_session_orchestrator` | Test exe | M1 pipeline state machine |
| `tst_inbound_playback` | Test exe | M1 inbound audio playback |
| `tst_constants` | Test exe | M1 constants sanity |
| `tst_connection_state` | Test exe | M1 connection state |
| `coverage` | Custom target | LCOV coverage report + threshold check |

#### Build Options

| Option | Default | Purpose |
|---|---|---|
| `PAIRION_COVERAGE` | OFF (ON in debug presets) | Enable `--coverage` flags |
| `PAIRION_WAKE_DIAGNOSTICS` | OFF | Enable Info-level wake classifier score logging |
| `PAIRION_CLIENT_NATIVE_TESTS` | OFF | Enable tests requiring real ONNX model files |
| `PAIRION_COVERAGE_THRESHOLD` | 100 | Minimum line coverage percent |

#### Build Commands

```bash
# Configure (macOS ARM64 debug, coverage enabled)
cmake --preset macos-arm64-debug

# Build
cmake --build build/macos-arm64-debug

# Test
ctest --preset macos-arm64-debug

# Coverage report (requires PAIRION_COVERAGE=ON)
cmake --build build/macos-arm64-debug --target coverage
```

#### cmake/check_coverage.cmake
Parses LCOV tracefile DA: records, computes hit/total ratio, fails build if below `PAIRION_COVERAGE_THRESHOLD` (default 100%).

#### cmake/filter_coverage.cmake
Runs `lcov --remove` to strip system headers (`/opt/homebrew/*`, `/usr/local/*`, `/usr/include/*`, `/Library/*`), autogen files (`*_autogen*`, `*/moc_*`, `*/qrc_*`), test code, and `main.cpp`.

---
### Section 4 — Configuration & Infrastructure Summary

#### CMakePresets.json — Preset Summary

| Preset Name | Platform | Build Type | Coverage | Arch |
|---|---|---|---|---|
| `macos-arm64-debug` | Darwin | Debug | ON | arm64 |
| `macos-arm64-release` | Darwin | Release | OFF | arm64 |
| `macos-x86_64-debug` | Darwin | Debug | ON | x86_64 |
| `macos-x86_64-release` | Darwin | Release | OFF | x86_64 |
| `linux-x86_64-debug` | Linux | Debug | ON | x86_64 |
| `linux-x86_64-release` | Linux | Release | OFF | x86_64 |
| `linux-arm64-debug` | Linux | Debug | ON | aarch64 |
| `linux-arm64-release` | Linux | Release | OFF | aarch64 |
| `windows-x86_64-debug` | Windows | Debug | N/A | x86_64 |
| `windows-x86_64-release` | Windows | Release | N/A | x86_64 |

- All presets use Ninja generator.
- Debug presets enable `-Wall -Wextra -Wpedantic -Werror`.
- Windows presets use `/W4 /WX` and vcpkg toolchain.
- macOS prefix path: `/opt/homebrew` (ARM64), `/usr/local` (x86_64).

#### CLAUDE.md — Behavioral Contract Summary

- Executor role only; never change architecture without approval.
- 100% code coverage mandatory (ships in same pass).
- Doxygen on every class and every public method.
- Build tool: CMake + Ninja. C++ Standard: C++20.
- Max 3 scope items per prompt.

#### CI/CD

No `.github/workflows/` directory — no automated CI pipeline configured.

#### Environment Variables / Config Files

- No `.env` files.
- Runtime config is via `QSettings` (see Section 12 — Settings).
- ONNX model files cached to `QStandardPaths::AppDataLocation/models/`.

#### Connection Map

```
Pairion-Client
    ↕ WebSocket (ws://localhost:18789/ws/v1)
Pairion-Server

    ↕ HTTP POST (http://localhost:18789/v1/logs)
Pairion-Server (log forwarding)
```

---
### Section 5 — Startup & Runtime Behavior

**Entry Point:** `src/main.cpp` → `int main(int argc, char *argv[])`

#### Initialization Sequence

1. **QGuiApplication** created; app name = "Pairion", version = `kClientVersion`.
2. **QNetworkAccessManager** created (parented to app).
3. **ConnectionState** singleton created (parented to app).
4. **Settings** singleton created (parented to app).
5. **Logger** created; log callback wired to `ConnectionState::appendLog()`; `Logger::install()` installs Qt message handler.
6. **DeviceIdentity** loaded from QSettings (or generated on first run).
7. **PairionWebSocketClient** created with server URL, device credentials, ConnectionState.
8. **PairionAudioCapture** created on main thread (macOS QAudioSource requires main thread).
9. **PairionOpusEncoder** created; moved to `EncoderThread` (QThread).
10. Audio device + sample rate configured from Settings; change signals wired.
11. **EncoderThread** and **InferenceThread** QThreads created (not started yet).
12. `ConnectionState` and `Settings` registered as QML singletons.
13. **QQmlApplicationEngine** loaded (`qrc:/qml/Main.qml`).
14. **ModelDownloader** created.
15. **Microphone permission** requested via `QMicrophonePermission` / `QGuiApplication::requestPermission()`.
16. **WebSocket** connects to server: `wsClient->connectToServer()`.
17. On `SessionOpened` → `ModelDownloader::checkAndDownload()` triggered.
18. Pipeline gate: `tryStartPipeline()` lambda waits for BOTH `micPermissionGranted && modelsReady`.
19. When both ready → **`initAudioPipeline()`** called:
    - Loads 4 ONNX models: `melspectrogram.onnx`, `embedding_model.onnx`, `hey_jarvis_v0.1.onnx`, `silero_vad.onnx`
    - Creates **OpenWakewordDetector** and **SileroVad**, moves both to InferenceThread
    - Creates **PairionAudioPlayback** (main thread)
    - Creates **AudioSessionOrchestrator** (main thread)
    - Wires signal connections: capture→encoder, capture→wakeDetector, capture→vad
    - Starts InferenceThread and EncoderThread
    - Queues `wakeDetector->warmup()` on InferenceThread
    - Calls `capture->start()` and `orchestrator->startListening()`

#### Threading Model

| Component | Thread |
|---|---|
| QML / PairionAudioCapture | Main thread |
| ConnectionState | Main thread |
| PairionWebSocketClient | Main thread |
| AudioSessionOrchestrator | Main thread |
| PairionAudioPlayback | Main thread |
| PairionOpusEncoder | EncoderThread |
| OpenWakewordDetector | InferenceThread |
| SileroVad | InferenceThread |

---
### Section 6 — Class Inventory

---

#### Module: audio/

---

**=== PairionAudioCapture (`src/audio/pairion_audio_capture.h`) ===**
Base Class: QObject
Q_OBJECT: YES
Namespace: `pairion::audio`

Public Methods:
- `PairionAudioCapture(QObject *parent)` — default system audio input
- `PairionAudioCapture(QIODevice *audioDevice, QObject *parent)` — injected device (testing)
- `~PairionAudioCapture()` — destructor
- `configuredDeviceId() const → QString` — configured device ID (empty = system default)
- `configuredSampleRate() const → int` — configured sample rate in Hz

Slots (public):
- `start()` — start capturing audio from the microphone
- `stop()` — stop capturing audio
- `configure(const QString &deviceId, int sampleRate)` — configure device/rate (restarts if running)

Signals:
- `audioFrameAvailable(const QByteArray &pcm20ms)` — 640-byte 20 ms PCM frame
- `captureError(const QString &reason)` — mic failure or device disconnection

Private Members:
- `m_audioSource: QAudioSource*` — Qt audio source
- `m_ioDevice: QIODevice*` — read device
- `m_accumulator: QByteArray` — frame accumulator
- `m_ownsSource: bool` — whether this object created the source
- `m_running: bool`
- `m_configuredDeviceId: QString`
- `m_configuredSampleRate: int` (default 16000)

Constants: `kSampleRate=16000`, `kChannels=1`, `kSampleSize=16`, `kFrameBytes=640`

Purpose: Microphone capture producing exact 20 ms PCM frames (640 bytes) at 16 kHz mono 16-bit.

---

**=== PairionAudioPlayback (`src/audio/pairion_audio_playback.h`) ===**
Base Class: QObject
Q_OBJECT: YES
Namespace: `pairion::audio`

Public Methods:
- `PairionAudioPlayback(QObject *parent)` — constructor
- `~PairionAudioPlayback()` — destructor
- `start()` — open QAudioSink, prepare for playback (idempotent)
- `stop()` — stop sink, drain buffer, reset speaking state
- `preparePlayback()` — pre-warm sink with silence before first frame

Slots (public):
- `handleOpusFrame(const QByteArray &opusFrame)` — decode Opus + queue PCM
- `handlePcmFrame(const QByteArray &pcm)` — write decoded PCM to sink
- `handleStreamEnd(const QString &reason)` — notify stream ended; calls stop()

Signals:
- `playbackError(const QString &message)` — decode failure or abnormal stream end
- `speakingStateChanged(const QString &state)` — "speaking" / "idle"

Private Members:
- `m_sink: QAudioSink*`, `m_audioDevice: QIODevice*`, `m_decoder: PairionOpusDecoder*`
- `m_jitterBuffer: QQueue<QByteArray>`, `m_silenceTimer: QTimer*`, `m_drainTimer: QTimer*`
- `m_isSpeaking: bool`

Constants: `kJitterMs=50`, `kSampleRate=48000`, `kSinkBufferBytes=kSampleRate*2*5` (5-second ring)

Purpose: QAudioSink-backed inbound audio playback with 50 ms jitter buffer and speaking-state transitions.

---

**=== PairionOpusEncoder (`src/audio/pairion_opus_encoder.h`) ===**
Base Class: QObject
Q_OBJECT: YES
Namespace: `pairion::audio`

Public Methods:
- `PairionOpusEncoder(QObject *parent)` — init libopus encoder
- `~PairionOpusEncoder()` — destroy encoder
- `isValid() const → bool` — whether encoder initialized successfully

Slots (public):
- `encodePcmFrame(const QByteArray &pcm20ms)` — encode 640-byte PCM to Opus

Signals:
- `opusFrameEncoded(const QByteArray &opusFrame)` — encoded output
- `encoderError(const QString &reason)` — encoding failure

Private Members:
- `m_encoder: OpusEncoder*`

Constants: `kSampleRate=16000`, `kChannels=1`, `kFrameSamples=320`, `kFrameBytes=640`, `kBitrate=28000`, `kComplexity=5`, `kMaxPacketBytes=4000`

Purpose: libopus wrapper configured for VOIP at 28 kbps, complexity 5; designed for worker-thread use.

---

**=== PairionOpusDecoder (`src/audio/pairion_opus_decoder.h`) ===**
Base Class: QObject
Q_OBJECT: YES
Namespace: `pairion::audio`

Public Methods:
- `PairionOpusDecoder(QObject *parent)` — init libopus decoder
- `~PairionOpusDecoder()` — destroy decoder
- `isValid() const → bool` — whether decoder initialized successfully

Slots (public):
- `decodeOpusFrame(const QByteArray &opusFrame)` — decode to PCM

Signals:
- `pcmFrameDecoded(const QByteArray &pcm)` — decoded PCM output
- `decoderError(const QString &reason)` — decoding failure

Constants: `kSampleRate=48000`, `kChannels=1`, `kMaxFrameSamples=5760`

Purpose: libopus decoder producing 16-bit mono PCM at 48 kHz.

---

**=== RingBuffer<T, Capacity> (`src/audio/ring_buffer.h`) ===**
Base Class: None (header-only template)
Q_OBJECT: NO
Namespace: `pairion::audio`

Public Methods:
- `push(const T &item) → bool` — enqueue; returns false if full
- `pop(T &item) → bool` — dequeue; returns false if empty
- `isEmpty() const → bool`
- `isFull() const → bool`
- `size() const → std::size_t`

Private Members: `m_data: std::array<T, Capacity>`, `m_writeIdx: std::atomic<size_t>`, `m_readIdx: std::atomic<size_t>`

Static assert: Capacity must be power of two. Memory ordering: release stores / acquire loads.

Purpose: Lock-free SPSC ring buffer for audio pipeline frame passing between capture and downstream threads.

---

#### Module: core/

---

**=== DeviceIdentity (`src/core/device_identity.h`) ===**
Base Class: QObject
Q_OBJECT: YES
Namespace: `pairion::core`

Public Methods:
- `DeviceIdentity(QObject *parent)` — loads from QSettings or generates
- `DeviceIdentity(const QString &deviceId, const QString &bearerToken, QObject *parent)` — explicit (testing)
- `deviceId() const → const QString&` — unique device identifier
- `bearerToken() const → const QString&` — bearer token for WS auth

Private Members: `m_deviceId: QString`, `m_bearerToken: QString`

Purpose: Manages persistent device UUID and bearer token via QSettings (generates on first launch).

---

**=== ModelDownloader (`src/core/model_downloader.h`) ===**
Base Class: QObject
Q_OBJECT: YES
Namespace: `pairion::core`

Public Methods:
- `ModelDownloader(QNetworkAccessManager *nam, QObject *parent)` — constructor
- `checkAndDownload()` — check cached models, download missing/corrupt files
- `static modelCacheDir() → QString` — absolute path to models cache directory
- `static modelPath(const QString &name) → QString` — absolute path to named model

Signals:
- `downloadProgress(int percent)` — 0-100
- `allModelsReady()` — all models verified and ready
- `downloadError(const QString &reason)` — failure

Private struct `ModelEntry`: `name: QString`, `url: QUrl`, `sha256: QByteArray`

Purpose: Downloads and SHA-256 verifies 4 ONNX model files from `model_manifest.json`; caches to AppDataLocation.

---

**=== OnnxInferenceSession (`src/core/onnx_session.h`) ===**
Base Class: None (abstract interface)
Q_OBJECT: NO
Namespace: `pairion::core`

Abstract method:
- `run(const std::vector<OnnxTensor> &inputs, const std::vector<std::string> &outputNames) → std::vector<OnnxOutput>` [pure virtual]

Purpose: Testability seam; subclassed by OrtInferenceSession (production) and MockOnnxSession (tests).

---

**=== OrtInferenceSession (`src/core/onnx_session.h`) ===**
Base Class: OnnxInferenceSession
Q_OBJECT: NO
Namespace: `pairion::core`

Public Methods:
- `OrtInferenceSession(const std::string &modelPath)` — loads .onnx from disk; throws `std::runtime_error` on failure
- `~OrtInferenceSession()` — PIMPL destructor
- `run(...)` override — synchronous ONNX inference

Private: Pimpl (`struct Impl`) hiding onnxruntime headers.

Purpose: Production ONNX inference session wrapping onnxruntime; sync inference, must be called from worker thread.

---

**=== OnnxTensor / OnnxOutput (structs, `src/core/onnx_session.h`) ===**
- `OnnxTensor`: `name: string`, `data: vector<float>`, `int64Data: vector<int64_t>`, `shape: vector<int64_t>`, `isInt64() const → bool`
- `OnnxOutput`: `data: vector<float>`, `shape: vector<int64_t>`

---

**=== constants.h (`src/core/constants.h`) ===**
Namespace: `pairion`

| Constant | Value | Purpose |
|---|---|---|
| `kDefaultServerUrl` | `ws://localhost:18789/ws/v1` | WebSocket server URL |
| `kDefaultRestBaseUrl` | `http://localhost:18789/v1` | REST base URL |
| `kHeartbeatIntervalMs` | 15000 | Heartbeat ping interval |
| `kHeartbeatDeadlineMs` | 10000 | Heartbeat pong deadline |
| `kLogFlushIntervalMs` | 30000 | Log flush interval |
| `kReconnectBackoffMs[]` | {1000,2000,4000,8000,15000,30000} | Reconnect backoff steps |
| `kReconnectBackoffSteps` | 6 | Number of backoff steps |
| `kPcmFrameBytes` | 640 | 20 ms PCM frame size |
| `kPreRollBytes` | 6400 | ~200 ms pre-roll buffer |
| `kStreamingTimeoutMs` | 30000 | Safety cap for runaway streams |
| `kWakeSuppressionMs` | 500 | False-wake suppression window |
| `kClientVersion` | injected via `version.h.in` | Client version string |

---

#### Module: pipeline/

---

**=== AudioSessionOrchestrator (`src/pipeline/audio_session_orchestrator.h`) ===**
Base Class: QObject
Q_OBJECT: YES
Namespace: `pairion::pipeline`

Enum: `State { Idle, AwaitingWake, Streaming, EndingSpeech }`

Constructor: Takes `PairionAudioCapture*`, `PairionOpusEncoder*`, `OpenWakewordDetector*`, `SileroVad*`, `PairionWebSocketClient*`, `ConnectionState*`, `PairionAudioPlayback*`, `QObject*`.

Public Methods:
- `AudioSessionOrchestrator(...)` — wires all signal connections, configures streaming timeout
- `state() const → State` — current pipeline state

Slots (public):
- `startListening()` — transition Idle → AwaitingWake
- `shutdown()` — stop all activity, transition to Idle

Signals:
- `wakeFired(const QString &streamId)` — wake word detected, stream started
- `streamEnded(const QString &streamId)` — speech ended, stream closed
- `pipelineError(const QString &message)` — pipeline error

Private Members:
- `m_capture`, `m_encoder`, `m_preRollEncoder`, `m_playback`, `m_wakeDetector`, `m_vad`, `m_wsClient`, `m_connState` — injected dependencies
- `m_state: State` — current state
- `m_activeStreamId: QString` — UUID of active stream
- `m_streamingTimeout: QTimer` — 30-second safety cap

Purpose: Central state machine coordinating the audio pipeline lifecycle.

---

#### Module: protocol/

---

**=== EnvelopeCodec (`src/protocol/envelope_codec.h`) ===**
Base Class: None (static methods only)
Q_OBJECT: NO
Namespace: `pairion::protocol`

Public Static Methods:
- `serialize(const OutboundMessage &msg) → QJsonObject`
- `serializeToString(const OutboundMessage &msg) → QString`
- `deserialize(const QJsonObject &obj) → std::optional<InboundMessage>`
- `deserializeFromString(const QString &json) → std::optional<InboundMessage>`

Thread-safe: all methods are stateless.

Purpose: JSON text-frame serializer/deserializer for all Pairion protocol message types.

---

**=== BinaryCodec (`src/protocol/binary_codec.h`) ===**
Base Class: None (static methods only)
Q_OBJECT: NO
Namespace: `pairion::protocol`

Constant: `kPrefixLength = 4`

Public Static Methods:
- `encodeBinaryFrame(const QString &streamId, const QByteArray &opusPayload) → QByteArray` — 4-byte prefix + payload
- `decodeBinaryFrame(const QByteArray &frame) → DecodedBinaryFrame` — split prefix and payload
- `streamIdToPrefix(const QString &streamId) → QByteArray` — first 4 bytes of UUID RFC 4122

struct `DecodedBinaryFrame`: `prefix: QByteArray`, `payload: QByteArray`

Purpose: Binary frame encoding/decoding — 4-byte stream ID prefix (UUID time_low field, big-endian) + Opus payload.

---

**=== Protocol Message Types (`src/protocol/messages.h`) ===**

**Outbound (Client → Server):**

| Struct | kType | Key Fields |
|---|---|---|
| `DeviceIdentify` | "DeviceIdentify" | deviceId, bearerToken, clientVersion |
| `HeartbeatPing` | "HeartbeatPing" | timestamp |
| `WakeWordDetected` | "WakeWordDetected" | timestamp, confidence (optional<double>) |
| `AudioStreamStartIn` | "AudioStreamStart" | streamId, codec="opus", sampleRate=16000 |
| `SpeechEnded` | "SpeechEnded" | streamId |
| `AudioStreamEndIn` | "AudioStreamEnd" | streamId, reason |
| `TextMessage` | "TextMessage" | text |

`OutboundMessage = std::variant<DeviceIdentify, HeartbeatPing, WakeWordDetected, AudioStreamStartIn, SpeechEnded, AudioStreamEndIn, TextMessage>`

**Inbound (Server → Client):**

| Struct | kType | Key Fields |
|---|---|---|
| `SessionOpened` | "SessionOpened" | sessionId, serverVersion |
| `SessionClosed` | "SessionClosed" | reason |
| `HeartbeatPong` | "HeartbeatPong" | timestamp |
| `ErrorMessage` | "Error" | code, message |
| `AgentStateChange` | "AgentStateChange" | state |
| `TranscriptPartial` | "TranscriptPartial" | text |
| `TranscriptFinal` | "TranscriptFinal" | text |
| `LlmTokenStream` | "LlmTokenStream" | delta |
| `ToolCallStarted` | "ToolCallStarted" | toolCallId, toolName, input (QJsonObject) |
| `ToolCallCompleted` | "ToolCallCompleted" | toolCallId, output (QJsonObject) |
| `AudioStreamStartOut` | "AudioStreamStart" | streamId, codec, sampleRate |
| `AudioStreamEndOut` | "AudioStreamEnd" | streamId, reason |
| `UnderBreathAck` | "UnderBreathAck" | acknowledgementType (optional<QString>) |
| `MapFocus` | "MapFocus" | lat, lon, label, zoom |
| `MapClear` | "MapClear" | (none) |

`InboundMessage = std::variant<SessionOpened, ..., MapFocus, MapClear>`

---

#### Module: state/

---

**=== ConnectionState (`src/state/connection_state.h`) ===**
Base Class: QObject
Q_OBJECT: YES
Namespace: `pairion::state`

Q_ENUM: `Status { Disconnected=0, Connecting, Connected, Reconnecting }`

Q_PROPERTY List:
- `status: Status` NOTIFY statusChanged
- `sessionId: QString` NOTIFY sessionIdChanged
- `serverVersion: QString` NOTIFY serverVersionChanged
- `reconnectAttempts: int` NOTIFY reconnectAttemptsChanged
- `recentLogs: QStringList` NOTIFY recentLogsChanged (trimmed to 10 entries)
- `transcriptPartial: QString` NOTIFY transcriptPartialChanged
- `transcriptFinal: QString` NOTIFY transcriptFinalChanged
- `llmResponse: QString` NOTIFY llmResponseChanged
- `agentState: QString` NOTIFY agentStateChanged
- `voiceState: QString` NOTIFY voiceStateChanged
- `backgroundContext: QString` NOTIFY backgroundContextChanged (default "earth")
- `mapFocusActive: bool` NOTIFY mapFocusChanged
- `mapFocusLat: double` NOTIFY mapFocusChanged
- `mapFocusLon: double` NOTIFY mapFocusChanged
- `mapFocusLabel: QString` NOTIFY mapFocusChanged
- `mapFocusZoom: QString` NOTIFY mapFocusChanged

Key Setters (slots):
- `setStatus(Status)`, `setSessionId(QString)`, `setServerVersion(QString)`, `setReconnectAttempts(int)`
- `appendLog(QString)` — trims to 10 entries
- `setTranscriptPartial(QString)`, `setTranscriptFinal(QString)`
- `setAgentState(QString)`, `setVoiceState(QString)`
- `setBackgroundContext(QString)`, `appendLlmToken(QString)`, `clearLlmResponse()`
- `setMapFocus(double lat, double lon, QString label, QString zoom)`, `clearMapFocus()`

Purpose: QML-exposed singleton aggregating all runtime state for the debug panel and HUD bindings.

---

#### Module: settings/

---

**=== Settings (`src/settings/settings.h`) ===**
Base Class: QObject
Q_OBJECT: YES
Namespace: `pairion::settings`

Q_PROPERTYs (READ/WRITE/NOTIFY):
- `wakeThreshold: double` — wake word confidence threshold
- `vadSilenceEndMs: int` — VAD silence duration before speech-ended
- `vadThreshold: double` — VAD speech probability threshold
- `audioInputDevice: QString` — QMediaDevices device name (empty = system default)
- `audioSampleRate: int` — capture sample rate in Hz

Setters (slots): `setWakeThreshold`, `setVadSilenceEndMs`, `setVadThreshold`, `setAudioInputDevice`, `setAudioSampleRate`

Purpose: QSettings-backed singleton exposing runtime-tunable audio and inference parameters.

---

#### Module: util/

---

**=== Logger (`src/util/logger.h`) ===**
Base Class: QObject
Q_OBJECT: YES
Namespace: `pairion::util`

Public Methods:
- `Logger(QNetworkAccessManager *nam, const QString &baseUrl, QObject *parent)` — constructor
- `~Logger()` — restores default Qt message handler
- `install()` — install Qt message handler + start 30-second flush timer
- `setSessionId(const QString &sessionId)` — include in log records
- `setLogCallback(std::function<void(const QString &)> cb)` — debug panel feed
- `flush()` — immediate HTTP POST of buffered records
- `pendingRecords() const → QJsonArray` — buffered records (for testing)
- `pendingCount() const → int` — count of pending records
- `static messageHandler(QtMsgType, QMessageLogContext&, const QString&)` — Qt message handler

Private Members:
- `m_nam: QNetworkAccessManager*`, `m_logUrl: QUrl`, `m_flushTimer: QTimer`
- `m_mutex: QMutex`, `m_buffer: QList<LogRecord>`
- `m_sessionId: QString`, `m_logCallback: std::function<void(const QString &)>`
- `static s_instance: Logger*` — singleton pointer for message handler

struct `LogRecord`: `timestamp, level, message, sessionId: QString`

Purpose: Centralized structured logger batching records and POSTing to `/v1/logs` every 30 seconds.

---

#### Module: wake/

---

**=== OpenWakewordDetector (`src/wake/open_wakeword_detector.h`) ===**
Base Class: QObject
Q_OBJECT: YES
Namespace: `pairion::wake`

Public Methods:
- `OpenWakewordDetector(OnnxInferenceSession *mel, OnnxInferenceSession *emb, OnnxInferenceSession *cls, double threshold, QObject *parent)` — constructor with 3 injected sessions

Slots (public):
- `warmup()` — pre-warm pipeline with silence
- `processPcmFrame(const QByteArray &pcm20ms)` — process 20 ms PCM frame

Signals:
- `wakeWordDetected(float score, const QByteArray &preRollBuffer)` — wake above threshold

Key Constants:
- `kMelChunkSamples=1280` (80 ms), `kMelContextSamples=480`, `kMelRowsPerChunk=8`
- `kMelFrameWidth=32`, `kMelFramesNeeded=76`, `kEmbFeaturesNeeded=16`, `kEmbFeatureSize=96`
- `kPreRollBytes=6400` (~200 ms), `kSuppressionMs=500`

Pipeline: PCM → float32 mel input (1760-sample sliding window) → melspectrogram model → rolling mel buffer → embedding model → rolling feature buffer → classifier model → score vs threshold.

Optional compile flag: `PAIRION_WAKE_DIAGNOSTICS` enables Info-level score logging per frame.

Purpose: Three-model openWakeWord ONNX pipeline for "Hey Jarvis" wake word detection.

---

#### Module: vad/

---

**=== SileroVad (`src/vad/silero_vad.h`) ===**
Base Class: QObject
Q_OBJECT: YES
Namespace: `pairion::vad`

Public Methods:
- `SileroVad(OnnxInferenceSession *session, double threshold, int silenceEndMs, QObject *parent)` — constructor

Slots (public):
- `processPcmFrame(const QByteArray &pcm20ms)` — process 20 ms PCM
- `reset()` — reset state machine and recurrent state

Signals:
- `speechStarted()` — speech detected above threshold
- `speechEnded()` — silence sustained for `silenceEndMs` ms

Enum (private): `VadState { Idle, Speaking, Trailing }`

Silero VAD v5 input: `[1, 512] float32` + state `[2, 1, 128] float32` + sr int64(16000). Output: `[1, 1] float32` probability + updated state.

Constants: `kInferenceSamples=512`, `kInferenceBytes=1024`, `kStateSize=256`

Purpose: Voice activity detection using Silero VAD v5; accumulates 512-sample chunks, maintains recurrent state.

---

#### Module: ws/

---

**=== PairionWebSocketClient (`src/ws/pairion_websocket_client.h`) ===**
Base Class: QObject
Q_OBJECT: YES
Namespace: `pairion::ws`

Public Methods:
- `PairionWebSocketClient(QUrl, QString deviceId, QString bearerToken, ConnectionState*, QObject*)` — constructor
- `connectToServer()` — initiate WebSocket connection
- `disconnectFromServer()` — disconnect and stop reconnection
- `sendMessage(const protocol::OutboundMessage &msg)` — serialize and send text frame
- `isConnected() const → bool`
- `setHeartbeatIntervalMs(int ms)` — override heartbeat interval (for testing)
- `sendBinaryFrame(const QByteArray &data)` — send raw binary frame

Signals:
- `sessionOpened(QString sessionId, QString serverVersion)`, `sessionClosed(QString reason)`, `serverError(QString code, QString message)`
- `disconnected()`, `inboundMessage(protocol::InboundMessage)`
- `binaryFrameReceived(QByteArray)`, `transcriptPartialReceived(QString)`, `transcriptFinalReceived(QString)`
- `agentStateReceived(QString)`, `llmTokenReceived(QString)`
- `audioStreamStartOutReceived(QString streamId)`, `audioStreamEndOutReceived(QString streamId, QString reason)`

Private Members:
- `m_socket: QWebSocket`, `m_serverUrl: QUrl`, `m_deviceId: QString`, `m_bearerToken: QString`
- `m_connState: ConnectionState*`, `m_heartbeatTimer: QTimer`, `m_reconnectTimer: QTimer`
- `m_reconnectAttempt: int`, `m_intentionalDisconnect: bool`

Reconnect backoff: `kReconnectBackoffMs[]` = {1000, 2000, 4000, 8000, 15000, 30000}.

Purpose: QWebSocket-based Pairion protocol client with DeviceIdentify, heartbeat, reconnect, and inbound message dispatch.

---
### Section 7 — QML Component Inventory

---

**=== Main.qml (`qml/Main.qml`) ===**
Root Type: `ApplicationWindow`
Properties: `hudActive: bool` (default true)
Key Handlers: F12 toggles HUD/debug panel; F11 toggles FPS counter; Escape quits app
Imports: `Pairion` 1.0 (ConnectionState, Settings singletons), HUD/, Debug/
Purpose: Root window, full-screen, dark background (#0a0e1a); switches between PairionHUD and DebugPanel.

---

**=== PairionHUD.qml (`qml/HUD/PairionHUD.qml`) ===**
Root Type: `Item`
Functions: `toggleFps()`, `focusPin(index)` — pan+zoom globe to pin index (-1 = zoom out)
State derivation: `hudState` = derived from `ConnectionState.agentState` and `ConnectionState.voiceState`
Structure: ContextBackground → HemisphereMap (worldMap) → RingSystem → HudLayout → FpsCounter
TopBar anchored at top.
Purpose: Full-screen cinematic HUD root; wires ConnectionState to all child components.

---

**=== HudLayout.qml (`qml/HUD/HudLayout.qml`) ===**
Root Type: `Item`
Properties: `hudState: string`
Layout constants: `margin=24`, `gap=20`
Panel map:
- Top row: Western hemisphere map (left) | Eastern hemisphere map (right) — `DashboardPanel` frames
- Bottom row: 4 × `DashboardPanel` (Inbox, News Headlines, Todo, Homestead Status)
- Footer: `TranscriptStrip` (full width)
Purpose: 2560×1440 reference geometry panel layout; transparent center for ring system visibility.

---

**=== RingSystem.qml (`qml/HUD/RingSystem.qml`) ===**
Root Type: `Item`
Properties: `hudState: string` (idle/listening/thinking/speaking/error)
Animated properties: `ringColor`, `opacityScale`, `speedScale`
State-driven: color, opacity, rotation speed, and arc span all animate 600 ms InOutQuad transitions
Rings: 5 concentric arcs via Qt Quick Shapes, independently rotating
Purpose: Visual state indicator — ring behavior encodes the current pipeline/agent state.

---

**=== HemisphereMap.qml (`qml/HUD/HemisphereMap.qml`) ===**
Root Type: `Item`
Properties: `pins: var[]` (each: {lat, lon, city, headline}), `activePinIndex: int` (-1 = auto-scroll)
Behavior: Three copies of equirectangular world map side-by-side; Timer-driven horizontal scroll for continuous globe rotation
Pin focus: animates map horizontally to center pin; zooms in; shortest-path wrapping prevents >180° spin
Longitude offset: 30°W shift so Atlantic is at x=50%
Purpose: World map with seamless globe scroll, pin focus pan+zoom, and news pin overlay.

---

**=== TopBar.qml (`qml/HUD/TopBar.qml`) ===**
Root Type: `Item`
Properties: `now: Date` (updated every second by Timer)
Displays: Live clocks for Dallas (local), London (UTC+1), Tokyo (UTC+9)
Styling: Dark navy (#0d1220) at 85% opacity, cyan text
Purpose: Full-width top bar with world city clocks.

---

**=== TranscriptStrip.qml (`qml/HUD/TranscriptStrip.qml`) ===**
Root Type: `Item`
Bindings: `ConnectionState.transcriptFinal`, `ConnectionState.transcriptPartial`
Styling: Dark translucent background (#0d1220 70%), 1px cyan border top
Purpose: Full-width footer showing final and partial STT transcript.

---

**=== DashboardPanel.qml (`qml/HUD/DashboardPanel.qml`) ===**
Root Type: `Item`
Properties: `title: string`, `content: alias` (default property → contentItem.data)
Styling: Dark translucent rectangle, 1px cyan border, uppercase letter-spaced header
Purpose: Reusable panel container used for all bottom panels and hemisphere map frames.

---

**=== DashboardPanels.qml (`qml/HUD/DashboardPanels.qml`) ===**
Root Type: `Item`
Constants: `mono: "Courier New"`, `cyan: "#00b4ff"`, `dim: "#e0e8f0"`
Content: Four side-by-side panels in a Row — News, Inbox, Todo, Homestead Status (static placeholder content)
Purpose: Four data panels in the bottom row of the HUD layout.

---

**=== ContextBackground.qml (`qml/HUD/ContextBackground.qml`) ===**
Root Type: `Item`
Properties: `context: string` ("earth" / "space")
Behavior: Crossfade 800 ms on context change; "earth" = transparent (globe shows through); "space" = animated star-field with bright accent stars and nebula haze
Star positions: deterministic pseudo-random by delegate index
Binds to: `ConnectionState.backgroundContext`
Purpose: Context-driven background layer behind the HUD ring system.

---

**=== FpsCounter.qml (`qml/HUD/FpsCounter.qml`) ===**
Root Type: `Item`
Properties: `shown: bool`, `frameCount: int`, `fps: real`, `lastMs: real`
Behavior: 250 ms polling timer samples frame timestamps; rolling-average FPS in top-right corner
Visible: Only when `shown = true` (F11 toggle)
Purpose: Developer FPS overlay; hidden by default.

---

**=== DebugPanel.qml (`qml/Debug/DebugPanel.qml`) ===**
Root Type: `Item`
Binds to: `ConnectionState` singleton (Pairion 1.0)
Displays: Connection status (colored dot), session ID, server version, voice pipeline state, transcript (partial + final), LLM response, agent state, last 10 log records
Purpose: M0/M1 developer debug panel; toggled by F12 from Main.qml.

---
### Section 8 — WebSocket Protocol Layer

#### PairionWebSocketClient — Connection Lifecycle

1. `connectToServer()` → `m_socket.open(m_serverUrl)`
2. `onConnected()` → send `DeviceIdentify{deviceId, bearerToken, clientVersion}` → `setStatus(Connecting)` → start heartbeat timer
3. On `SessionOpened` → `setStatus(Connected)`, `setReconnectAttempts(0)`, emit `sessionOpened`
4. `onHeartbeatTick()` → send `HeartbeatPing{timestamp}`, check pong deadline
5. `onDisconnected()` → stop heartbeat → if not intentional → `scheduleReconnect()`
6. `scheduleReconnect()` → `setStatus(Reconnecting)`, increment attempt, start reconnect timer with exponential backoff
7. `disconnectFromServer()` → set `m_intentionalDisconnect=true` → close socket

#### EnvelopeCodec — Message Format

JSON text frames. Envelope format:
```json
{ "type": "MessageTypeName", ...fields... }
```
- Serialization: `std::visit` on `OutboundMessage` variant → build QJsonObject with `"type"` field + message-specific fields
- Deserialization: read `"type"` field → dispatch to correct parser → return `std::optional<InboundMessage>`
- Returns `std::nullopt` on unknown type or parse error

#### BinaryCodec — Binary Frame Format

```
[4 bytes: stream ID prefix][N bytes: Opus payload]
```
- Prefix = first 4 bytes of stream ID UUID in RFC 4122 raw form (big-endian time_low field)
- Server uses prefix to route binary frames to correct stream session
- `encodeBinaryFrame(streamId, opusPayload)` → `streamIdToPrefix(streamId) + opusPayload`
- `decodeBinaryFrame(frame)` → split at byte 4; returns empty payload if frame < 4 bytes

#### Protocol Message Types

See Section 6 — Protocol Message Types for complete list.

#### JSON Envelope Examples

Outbound DeviceIdentify:
```json
{ "type": "DeviceIdentify", "deviceId": "uuid", "bearerToken": "token", "clientVersion": "0.3.0" }
```

Outbound WakeWordDetected:
```json
{ "type": "WakeWordDetected", "timestamp": "2026-04-21T22:36:48.000Z", "confidence": 0.87 }
```

Outbound AudioStreamStart:
```json
{ "type": "AudioStreamStart", "streamId": "uuid", "codec": "opus", "sampleRate": 16000 }
```

Inbound SessionOpened:
```json
{ "type": "SessionOpened", "sessionId": "session-uuid", "serverVersion": "1.0.0" }
```

---
### Section 9 — Audio Pipeline

---

**=== PairionAudioCapture ===**
Purpose: Microphone capture producing exact 20 ms PCM frames at 16 kHz mono 16-bit.
Input: QAudioSource (system mic or injected QIODevice)
Output: `audioFrameAvailable(QByteArray pcm20ms)` — 640-byte frames
Thread: Main thread (macOS QAudioSource requires main thread)
Key Methods: `start()`, `stop()`, `configure(deviceId, sampleRate)`

---

**=== PairionOpusEncoder ===**
Purpose: Encodes 20 ms PCM frames to Opus at 28 kbps, VOIP mode.
Input: `encodePcmFrame(QByteArray pcm20ms)` — 640 bytes
Output: `opusFrameEncoded(QByteArray opusFrame)` — variable-length Opus
Thread: EncoderThread (moved via `moveToThread`)
Key Methods: `encodePcmFrame()`, `isValid()`

---

**=== PairionOpusDecoder ===**
Purpose: Decodes Opus frames to 16-bit mono PCM at 48 kHz.
Input: `decodeOpusFrame(QByteArray opusFrame)`
Output: `pcmFrameDecoded(QByteArray pcm)`
Thread: Main thread (used by PairionAudioPlayback)
Key Methods: `decodeOpusFrame()`, `isValid()`

---

**=== PairionAudioPlayback ===**
Purpose: Plays inbound server audio (TTS) via QAudioSink with 50 ms jitter buffer.
Input: `handleOpusFrame(opusFrame)` or `handlePcmFrame(pcm)`
Output: `speakingStateChanged("speaking" | "idle")`, `playbackError(message)`
Thread: Main thread
Key Methods: `start()`, `stop()`, `preparePlayback()`, `handleOpusFrame()`, `handleStreamEnd()`

---

**=== RingBuffer<T, Capacity> ===**
Purpose: Lock-free SPSC ring buffer for audio frame passing between threads.
Input: `push(item)` from producer thread
Output: `pop(item)` from consumer thread
Thread: Cross-thread (SPSC, not used directly in runtime — serves as utility)
Key Methods: `push()`, `pop()`, `isEmpty()`, `isFull()`, `size()`

---

**Audio Pipeline Data Flow:**

```
[Mic] → PairionAudioCapture (main)
          ↓ audioFrameAvailable (queued)
          ├→ PairionOpusEncoder (EncoderThread)
          │     ↓ opusFrameEncoded
          │     └→ AudioSessionOrchestrator.onOpusFrameEncoded
          │           └→ PairionWebSocketClient.sendBinaryFrame [if Streaming]
          │
          ├→ OpenWakewordDetector (InferenceThread)
          │     ↓ wakeWordDetected(score, preRollBuffer)
          │     └→ AudioSessionOrchestrator.onWakeWordDetected
          │
          └→ SileroVad (InferenceThread)
                ↓ speechEnded
                └→ AudioSessionOrchestrator.onSpeechEnded

[Server binary] → PairionWebSocketClient.binaryFrameReceived
                  └→ AudioSessionOrchestrator.onInboundAudio
                       └→ PairionAudioPlayback.handleOpusFrame
```

---
### Section 10 — State Machine (AudioSessionOrchestrator)

#### State Enum

```cpp
enum class State { Idle, AwaitingWake, Streaming, EndingSpeech };
```
State string names written to `ConnectionState::setVoiceState()`: "idle", "awaiting_wake", "streaming", "ending_speech".

#### State Transition Table

| From | To | Trigger |
|---|---|---|
| Idle | AwaitingWake | `startListening()` |
| AwaitingWake | Streaming | `onWakeWordDetected()` |
| Streaming | Idle | `onSpeechEnded()` → `endStream("normal")` → `startListening()` |
| Streaming | Idle | `onStreamingTimeout()` → `endStream("timeout")` → `startListening()` |
| Streaming | Idle | `onTtsPlaybackStarted()` → `endStream("normal")` → `startListening()` |
| Any | Idle | `shutdown()` |

After `endStream()`, `startListening()` is called automatically → immediately transitions Idle → AwaitingWake.

#### Constructor Signal/Slot Wiring

```
m_wakeDetector.wakeWordDetected → onWakeWordDetected()
m_vad.speechEnded               → onSpeechEnded()
m_encoder.opusFrameEncoded      → onOpusFrameEncoded()
m_playback.speakingStateChanged → m_connState.setAgentState()
m_playback.speakingStateChanged → [lambda: onTtsPlaybackStarted() if state=="speaking"]
m_streamingTimeout.timeout      → onStreamingTimeout()
m_wsClient.binaryFrameReceived  → onInboundAudio()
m_wsClient.audioStreamStartOut  → onInboundAudioStreamStart()
m_wsClient.audioStreamEndOut    → onInboundStreamEnd()
```

Also creates `m_preRollEncoder` (a second `PairionOpusEncoder`, lives on main thread, not moved to EncoderThread) for synchronous pre-roll encoding.

#### transitionTo() Behavior

Guards against no-op (same state). Updates `m_state` and calls `m_connState->setVoiceState(kStateNames[...])`.

#### Key Method Logic

**startListening():**
- Returns if `m_state != Idle`
- Calls `transitionTo(AwaitingWake)`

**shutdown():**
- Stops streaming timeout timer
- Calls `transitionTo(Idle)` (unconditional)

**onWakeWordDetected(float score, QByteArray preRollBuffer):**
- Returns if `m_state != AwaitingWake`
- Generates new stream UUID
- Sends `WakeWordDetected` + `AudioStreamStartIn` envelopes
- Encodes pre-roll PCM via `m_preRollEncoder` using `Qt::DirectConnection` (synchronous), sends each frame as binary
- Calls `transitionTo(Streaming)`
- Starts 30-second timeout timer
- Calls `m_vad->reset()`
- Emits `wakeFired(streamId)`

**onSpeechEnded():**
- Returns if `m_state != Streaming`
- Calls `endStream("normal")`

**onOpusFrameEncoded(QByteArray opusFrame):**
- Returns if `m_state != Streaming`
- Encodes binary frame via `BinaryCodec::encodeBinaryFrame(m_activeStreamId, opusFrame)`
- Calls `m_wsClient->sendBinaryFrame(binaryFrame)`

**endStream(QString reason):**
- Stops timeout timer
- Sends `SpeechEnded` + `AudioStreamEndIn` envelopes
- Clears `m_activeStreamId`
- Calls `transitionTo(Idle)` then `startListening()`
- Emits `streamEnded(streamId)`

**onTtsPlaybackStarted():**
- Returns if `m_state != Streaming`
- Calls `endStream("normal")` to prevent mic loopback during TTS

---
### Section 11 — Wake Word & VAD

#### OpenWakewordDetector — Three-Model Pipeline

**Class:** `pairion::wake::OpenWakewordDetector`
**Purpose:** openWakeWord wake word detection using melspectrogram → embedding → classifier ONNX chain.
**Input:** `processPcmFrame(QByteArray pcm20ms)` — 640 bytes, 16-bit mono 16 kHz, via InferenceThread
**Output:** `wakeWordDetected(float score, QByteArray preRollBuffer)` — emitted when classifier score > threshold

**Pipeline Detail:**
1. PCM int16 → float32, normalize to [-1, 1]
2. Accumulate 1280 samples (80 ms chunks)
3. On 1280 samples: run melspectrogram model with 1760-sample window (1280 + 480 context)
4. Append 8 new mel rows (32 floats each) to `m_melBuffer`
5. When `m_melBuffer >= 76` rows: run embedding model → 96-float feature vector
6. Append to `m_embFeatures`
7. When `m_embFeatures >= 16`: run classifier → score
8. If score > `m_threshold` and suppression not active:
   - Emit `wakeWordDetected(score, preRollBuffer)`
   - Start 500 ms suppression timer

**Pre-roll:** Maintains a 6400-byte (~200 ms) circular PCM buffer; attached to wake event.

**Suppression:** `m_suppressionTimer` (QElapsedTimer): 500 ms cooldown after detection.

**Warm-up:** `warmup()` runs ~2 seconds of silence through all three models to initialize state.

---

#### SileroVad — State Machine

**Class:** `pairion::vad::SileroVad`
**Purpose:** Voice activity detection using Silero VAD v5 ONNX model.
**Input:** `processPcmFrame(QByteArray pcm20ms)` — 640 bytes, 16-bit mono; accumulates to 512 samples
**Output:** `speechStarted()`, `speechEnded()`

**State Machine:** `VadState { Idle, Speaking, Trailing }`

| State | Condition | Transition |
|---|---|---|
| Idle | prob > threshold | → Speaking, emit speechStarted |
| Speaking | prob < threshold | → Trailing, start silence timer |
| Trailing | prob > threshold | → Speaking (barge-back-in) |
| Trailing | silence timer expires (silenceEndMs) | → Idle, emit speechEnded |

**Model inputs:**
- `input`: [1, 512] float32 (512 PCM samples normalized)
- `state`: [2, 1, 128] float32 (recurrent state, 256 floats)
- `sr`: scalar int64 = 16000

**Recurrent state:** Updated from model output `stateN` on every inference call.

**reset():** Clears state machine back to Idle, zeroes recurrent state.

---
### Section 12 — Connection & Application State

#### ConnectionState — QML-Exposed Singleton

**Namespace:** `pairion::state`
**Registered as:** `qmlRegisterSingletonInstance("Pairion", 1, 0, "ConnectionState", connState)`

**Status Enum:**
- `Disconnected = 0` — not connected
- `Connecting` — DeviceIdentify sent, awaiting SessionOpened
- `Connected` — session active
- `Reconnecting` — reconnect backoff in progress

**Key State Fields:**

| Property | Type | Purpose |
|---|---|---|
| `status` | Status | WebSocket connection status |
| `sessionId` | QString | Active session UUID from server |
| `serverVersion` | QString | Server version from SessionOpened |
| `reconnectAttempts` | int | Count since last successful connect |
| `recentLogs` | QStringList | Last 10 log entries (trimmed) |
| `transcriptPartial` | QString | In-progress STT text |
| `transcriptFinal` | QString | Final STT text |
| `llmResponse` | QString | Accumulated LLM token stream |
| `agentState` | QString | idle / listening / thinking / speaking |
| `voiceState` | QString | idle / awaiting_wake / streaming / ending_speech |
| `backgroundContext` | QString | "earth" or "space" — drives ContextBackground |
| `mapFocusActive` | bool | Whether server has set a map focus |
| `mapFocusLat/Lon` | double | Geographic coordinates of map focus |
| `mapFocusLabel` | QString | Human-readable location label |
| `mapFocusZoom` | QString | continent / country / region / city |

**HUD Bindings:** PairionHUD derives `hudState` from a combination of `agentState` and `voiceState`. RingSystem and other components bind to this derived state.

---

#### Settings — Runtime Configuration

**Namespace:** `pairion::settings`
**Registered as:** `qmlRegisterSingletonInstance("Pairion", 1, 0, "Settings", settings)`

**Defaults (from QSettings):**

| Setting | Purpose |
|---|---|
| `wakeThreshold` | Wake word classifier confidence threshold (default likely 0.5) |
| `vadSilenceEndMs` | ms of silence before speech-ended (default likely 800) |
| `vadThreshold` | VAD speech probability threshold (default likely 0.5) |
| `audioInputDevice` | Device name; empty = system default |
| `audioSampleRate` | Capture sample rate Hz (default likely 16000) |

Values persist across launches via `QSettings(organizationName="Pairion", applicationName="Pairion")`.

---
### Section 13 — Exception Handling & Error Patterns

#### Error Propagation Patterns

1. **Signals** — primary error propagation mechanism:
   - `pipelineError(QString message)` — AudioSessionOrchestrator (declared but not emitted at M1)
   - `captureError(QString reason)` — PairionAudioCapture
   - `encoderError(QString reason)` — PairionOpusEncoder
   - `decoderError(QString reason)` — PairionOpusDecoder
   - `playbackError(QString message)` — PairionAudioPlayback
   - `serverError(QString code, QString message)` — PairionWebSocketClient
   - `downloadError(QString reason)` — ModelDownloader

2. **Return codes** — `bool isValid()` on PairionOpusEncoder and PairionOpusDecoder
3. **std::optional** — EnvelopeCodec returns `std::optional<InboundMessage>` (nullopt on parse error)
4. **std::exception** — OrtInferenceSession constructor throws `std::runtime_error` on model load failure; caught in `initAudioPipeline()` with `qCritical()` + ConnectionState log entry

#### Qt Logging Category Usage

All modules use `Q_LOGGING_CATEGORY` mechanism — no raw `std::cout` or `printf`:

| Category | Module |
|---|---|
| `pairion.pipeline` | AudioSessionOrchestrator |
| `pairion.ws` | PairionWebSocketClient |
| `pairion.audio.capture` | PairionAudioCapture |
| `pairion.audio.encoder` | PairionOpusEncoder |
| `pairion.audio.decoder` | PairionOpusDecoder |
| `pairion.audio.playback` | PairionAudioPlayback |
| `pairion.wake` | OpenWakewordDetector |
| `pairion.vad` | SileroVad |
| `pairion.core.downloader` | ModelDownloader |

63 uses of `Q_LOGGING_CATEGORY`, `qCInfo`, `qCDebug`, `qCWarning` across src/.

#### Centralized Logger

`pairion::util::Logger` installs itself as the Qt message handler, intercepting all Qt logging output. Buffers records in memory, flushes every 30 seconds to `POST /v1/logs`. Also forwards to `ConnectionState::appendLog()` for the debug panel.

#### Error Recovery

- WebSocket disconnects: exponential backoff reconnect (1s → 30s cap)
- ONNX model load failure: logs to ConnectionState, returns from `initAudioPipeline()` (no pipeline started)
- Streaming timeout: 30-second timer calls `endStream("timeout")` (LCOV_EXCL region — not exercisable in unit tests)
- Microphone denial: logs warning to ConnectionState

---
### Section 14 — Test Suite

#### Test Files

| File | Class | Test Count |
|---|---|---|
| `test/tst_message_codec.cpp` | TestMessageCodec | 26 |
| `test/tst_log_batching.cpp` | TestLogBatching | 11 |
| `test/tst_ws_client_state_machine.cpp` | TestWsClientStateMachine | 15 |
| `test/tst_device_identity.cpp` | TestDeviceIdentity | 5 |
| `test/tst_integration.cpp` | TestIntegration | 7 |
| `test/tst_binary_frame.cpp` | TestBinaryFrame | 6 |
| `test/tst_settings.cpp` | TestSettings | 11 |
| `test/tst_ring_buffer.cpp` | TestRingBuffer | 7 |
| `test/tst_opus_codec.cpp` | TestOpusCodec | 8 |
| `test/tst_wake_detector.cpp` | TestWakeDetector | 7 |
| `test/tst_silero_vad.cpp` | TestSileroVad | 6 |
| `test/tst_model_downloader.cpp` | TestModelDownloader | 4 |
| `test/tst_audio_session_orchestrator.cpp` | TestAudioSessionOrchestrator | 12 |
| `test/tst_inbound_playback.cpp` | TestInboundPlayback | 10 |
| `test/tst_constants.cpp` | TestConstants | 3 |
| `test/tst_connection_state.cpp` | TestConnectionState | 10 |

**Total: 16 test files, ~148 test slots**

---

#### Test Detail

**TestMessageCodec** — `tst_message_codec.cpp`
Tests:
- `serializeDeviceIdentify()`, `serializeHeartbeatPing()`, `serializeWakeWordDetected()`, `serializeWakeWordDetectedNoConfidence()`, `serializeAudioStreamStartIn()`, `serializeSpeechEnded()`, `serializeAudioStreamEndIn()`, `serializeTextMessage()`, `serializeToStringProducesValidJson()` — outbound serialization
- `deserializeSessionOpened()`, `deserializeSessionClosed()`, `deserializeHeartbeatPong()`, `deserializeError()`, `deserializeAgentStateChange()`, `deserializeTranscriptPartial()`, `deserializeTranscriptFinal()`, `deserializeLlmTokenStream()`, `deserializeToolCallStarted()`, `deserializeToolCallCompleted()`, `deserializeAudioStreamStartOut()`, `deserializeAudioStreamEndOut()`, `deserializeUnderBreathAck()`, `deserializeUnderBreathAckNoType()`, `deserializeMapFocus()`, `deserializeMapClear()` — inbound deserialization
Dependencies mocked: None (pure codec)

**TestLogBatching** — `tst_log_batching.cpp`
Tests: `initiallyEmpty()`, `messageHandlerEnqueues()`, `logLevelMapping()`, `sessionIdIncluded()`, `flushClearsBuffer()`, `flushEmptyBufferIsNoop()`, `logCallbackInvoked()`, `flushTimerStartsOnInstall()`, `messageHandlerWithNullInstance()`, `recordsWithoutSessionId()`, `flushWithSessionId()`
Dependencies mocked: QNetworkAccessManager stub

**TestWsClientStateMachine** — `tst_ws_client_state_machine.cpp`
Tests: `connectAndIdentify()`, `statusTransitions()`, `heartbeatPingSent()`, `reconnectOnDisconnect()`, `intentionalDisconnectNoReconnect()`, `serverErrorForwarded()`, `sessionClosedForwarded()`, `binaryFrameReceived()`, `inboundMessageSignalEmitted()`, `unknownMessageHandled()`, `transcriptMessagesForwarded()`, `llmTokenStreamForwarded()`, `agentStateThinkingClearsLlm()`, `appendLogAndRecentLogs()`, `appendLogTrimsAt50()`, `sendMessageWorks()`
Dependencies mocked: MockServer

**TestDeviceIdentity** — `tst_device_identity.cpp`
Tests: `init()`, `explicitConstruction()`, `firstLaunchCreatesCredentials()`, `subsequentLaunchReusesCredentials()`, `partialStateRegeneratesToken()`, `partialStateRegeneratesDeviceId()`

**TestIntegration** — `tst_integration.cpp`
Tests: `fullHappyPath()`, `disconnectAndReconnect()`, `binaryFrameRouting()`, `inboundProtocolMessages()`, `connectionStateLogAccumulation()`, `connectionStateSignals()`, `mockServerUtilities()`
Dependencies mocked: MockServer

**TestBinaryFrame** — `tst_binary_frame.cpp`
Tests: `encodeDecodeRoundTrip()`, `prefixIsFirst4BytesOfUuid()`, `emptyPayloadRoundTrip()`, `decodeTooShortFrame()`, `nullUuidProducesZeroPrefix()`, `deterministicPrefix()`

**TestSettings** — `tst_settings.cpp`
Tests: `init()`, `defaultValues()`, `setterPersists()`, `settersEmitSignals()`, `noEmitOnSameValue()`, `configurePropagatesToCapture()`, `configureFallbackRate()`, `defaultConstructorCreated()`, `startStopWithBuffer()`, `configureRestartsCapture()`, `audioDataReadyEmitsFrame()`

**TestRingBuffer** — `tst_ring_buffer.cpp`
Tests: `pushPopRoundTrip()`, `startsEmpty()`, `fullBufferRejectsPush()`, `emptyBufferRejectsPop()`, `wrapAround()`, `sizeTracking()`, `worksWithQByteArray()`

**TestOpusCodec** — `tst_opus_codec.cpp`
Tests: `encoderInitializes()`, `decoderInitializes()`, `encodeProducesOutput()`, `decodeProducesCorrectSampleCount()`, `roundTripPreservesLength()`, `encodeRejectsWrongSize()`, `decodeRejectsEmpty()`, `decodeHandlesCorruptData()`

**TestWakeDetector** — `tst_wake_detector.cpp`
Tests: `belowThresholdDoesNotFire()`, `aboveThresholdFires()`, `suppressionPreventsRefire()`, `suppressionExpires()`, `preRollBufferAttached()`, `allSessionsCalled()`, `warmupDoesNotCrash()`
Dependencies mocked: MockOnnxSession (for mel, embedding, classifier sessions)

**TestSileroVad** — `tst_silero_vad.cpp`
Tests: `silenceNoSpeech()`, `speechStartedEmitted()`, `speechEndedAfterSilence()`, `bargeBackInCancelsEnd()`, `resetClearsState()`, `recurrentStateUpdated()`
Dependencies mocked: MockOnnxSession

**TestModelDownloader** — `tst_model_downloader.cpp`
Tests: `cacheDirNonEmpty()`, `modelPathAppendsName()`, `missingFilesTriggersDownload()`, `hashMismatchTriggersRedownload()`

**TestAudioSessionOrchestrator** — `tst_audio_session_orchestrator.cpp`
Tests: `initialStateIsIdle()`, `startListeningTransitions()`, `wakeTransitionsToStreaming()`, `speechEndedTransitions()`, `shutdownStops()`, `opusFrameForwarded()`, `startListeningNoOpWhenNotIdle()`, `wakeWordGuardWhenNotAwaitingWake()`, `speechEndedGuardWhenNotStreaming()`, `opusFrameGuardWhenNotStreaming()`, `shutdownWhenAlreadyIdleIsNoOp()`, `preRollEncodedAndSentAsBinaryFrames()`
Dependencies mocked: MockServer

**TestInboundPlayback** — `tst_inbound_playback.cpp`
Tests: `inboundAudioSetsAgentStateSpeaking()`, `inboundStreamEndNormalSetsAgentStateIdle()`, `preparePlaybackDoesNotCrash()`, `handleStreamEndAbnormalEmitsError()`, `handleStreamEndNormalNoError()`, `startRestartsBehavior()`, `stopIdempotent()`, `preparePlaybackAfterStopRestartsDevice()`, `silenceTimerTransitionsToIdle()`, `truncatedBinaryFrameDropped()`
Dependencies mocked: MockServer

**TestConstants** — `tst_constants.cpp`
Tests: `clientVersionIsNonEmpty()`, `clientVersionIsExpected()`, `wakeSuppressionMsIsPositive()`

**TestConnectionState** — `tst_connection_state.cpp`
Tests: `backgroundContextDefault()`, `backgroundContextSetAndGet()`, `backgroundContextEmitsSignal()`, `backgroundContextNoEmitOnSameValue()`, `backgroundContextRoundTrip()`, `mapFocusDefaultInactive()`, `mapFocusSetAndGet()`, `mapFocusEmitsSignal()`, `mapFocusClearDeactivates()`, `mapFocusClearNoEmitWhenAlreadyInactive()`

---

#### Mock Classes

**MockOnnxSession** (`test/mock_onnx_session.h`)
- Implements `OnnxInferenceSession` with a queue of pre-programmed outputs
- `enqueueOutput(vector<OnnxOutput>)` — queue responses for next run() call
- `runCount() const → int` — number of run() calls
- `lastInputs() const → vector<OnnxTensor>&` — inputs from most recent run()
- Returns zero-filled defaults when queue is empty

**MockServer** (`test/mock_server.h/.cpp`)
- `QWebSocketServer`-based mock implementing Pairion protocol
- Listens on random available port
- Auto-responds: DeviceIdentify → SessionOpened; HeartbeatPing → HeartbeatPong
- `sendToClient(QString json)`, `sendToClient(QJsonObject)`, `sendBinaryToClient(QByteArray)`
- `receivedMessages()`, `receivedBinaryMessages()`, `hasClient()`
- `setMessageHandler(func)` — override default behavior
- Signals: `clientConnected()`, `clientDisconnected()`, `messageReceived()`

---

#### Coverage

**Last recorded coverage:** 967/967 lines = 100.0%

Coverage enforced via LCOV. `cmake/check_coverage.cmake` fails the build if coverage falls below 100%.

Exclusions (via `LCOV_EXCL_START/STOP` or `LCOV_EXCL_LINE`):
- `onStreamingTimeout()` — 30 s timeout, not exercisable in unit tests
- `Q_ENUM(Status)` macro internals (unreachable by application code)
- `OnnxTensor` struct (DTO, tested indirectly)

---
### Section 15 — Utility & Shared Components

#### pairion_core Static Library

All production source files (excluding `main.cpp`) are compiled into `pairion_core` static library. This allows all test executables to link against `pairion_core + Qt6::Test` without relinking everything.

#### version.h (generated)

`src/core/version.h.in` is processed by `configure_file` at CMake configure time:
- Injects `kClientVersion` constant from `project(pairion VERSION 0.3.0)`
- Output: `build/<preset>/src/core/version.h`
- Included by `constants.h`

#### resources.qrc

Root resource file compiled into the `pairion` executable:
- Embeds all `qml/**/*.qml` files under `qrc:/qml/`
- Embeds `resources/model_manifest.json` under `qrc:/resources/`

#### Platform Info.plist

`platform/macos/Info.plist` — macOS bundle metadata (microphone usage description, bundle identifier, etc.).

---

### Section 16 — Database Layer

No database layer — this is a desktop client application.

---

### Section 17 — Message Broker

No message broker — communication is via WebSocket (see Section 8).

---

### Section 18 — Cache Layer

No cache layer — stateless desktop client. ONNX models are cached to `QStandardPaths::AppDataLocation/models/` by ModelDownloader, but this is file-system caching, not a cache service.

---

### Section 19 — Environment Variable Inventory

No environment variables are used at runtime. The only preprocessor-level flag is:

| Flag | Type | Purpose |
|---|---|---|
| `PAIRION_WAKE_DIAGNOSTICS` | Compile-time `#ifdef` | Enables Info-level wake classifier score logging per frame. Set via CMake option `PAIRION_WAKE_DIAGNOSTICS=ON`. Default OFF. |
| `PAIRION_CLIENT_NATIVE_TESTS` | Compile-time `#ifdef` | Enables test cases requiring real ONNX model files on disk. Default OFF. |

No use of `getenv()`, `qgetenv()`, or `QProcessEnvironment` in production code.

---

### Section 20 — Service Dependency Map

#### Pairion-Server (required)

| Endpoint | Protocol | Purpose |
|---|---|---|
| `ws://localhost:18789/ws/v1` | WebSocket | Main protocol: audio streaming, session, transcripts, LLM, TTS |
| `http://localhost:18789/v1/logs` | HTTP POST | Batched structured log forwarding |

Both URLs are defined in `src/core/constants.h` as `kDefaultServerUrl` and `kDefaultRestBaseUrl`.

#### ONNX Models (downloaded on first launch)

| Model | Source | SHA-256 |
|---|---|---|
| `melspectrogram.onnx` | github.com/dscripka/openWakeWord v0.5.1 | ba2b0e0f... |
| `embedding_model.onnx` | github.com/dscripka/openWakeWord v0.5.1 | 70d16429... |
| `hey_jarvis_v0.1.onnx` | github.com/dscripka/openWakeWord v0.5.1 | 94a13cfe... |
| `silero_vad.onnx` | github.com/snakers4/silero-vad v5.1.2 | 2623a295... |

Cache location: `QStandardPaths::AppDataLocation + "/models/"` (e.g. `~/Library/Application Support/Pairion/Pairion/models/` on macOS).

Models are downloaded once; subsequent launches verify SHA-256 and skip download if matching.

---

### Section 21 — Known Technical Debt & Issues

#### TODO/FIXME Scan

**RESULT: Zero TODO/FIXME/HACK/TEMP items found in src/.**

#### Known Architectural Limitations

1. **Conversation Mode Reverted (PC-CONV-revert)**
   - Commit: `05b16cc` — reverted all conversation mode work (PC-CONV-001 through PC-CONV-004)
   - Root cause: VAD loopback incompatibility without Acoustic Echo Cancellation (AEC)
   - When TTS plays through speakers, the microphone picks it up, VAD triggers speech detection, creating an endless loop
   - Current workaround: mic upload stream is terminated (`endStream("normal")`) when TTS playback starts (`onTtsPlaybackStarted()`)
   - Fix requires: AEC dependency (system or library) to subtract speaker output from mic input before VAD processing
   - **Status: BLOCKED on AEC implementation**

2. **No CI/CD Pipeline**
   - No `.github/workflows/` — builds and tests are run manually
   - Risk: no automated regression detection on push

3. **Hardcoded localhost URLs**
   - `kDefaultServerUrl = "ws://localhost:18789/ws/v1"` and `kDefaultRestBaseUrl` are compile-time constants
   - No runtime override mechanism (no config file, no env var)
   - Requires recompile to point at a different server

4. **WebSocket uses plain ws:// (no TLS)**
   - Both server URL and REST URL use unencrypted connections
   - Acceptable for localhost development; would require `wss://` for production deployment

5. **Wake/VAD on InferenceThread (comment notes future consideration)**
   - `main.cpp` comment: "move to worker thread at M2 if profiling shows main-thread pressure" — already done at M1 (both on InferenceThread)
   - Comment is now stale but harmless

---

### Section 22 — Security Vulnerability Scan

```
Snyk CLI: available but not run during this audit (desktop client, no server-side components, no package.json/requirements.txt to scan).
```

**Manual Security Assessment:**

| Check | Result |
|---|---|
| SSL/TLS (wss://) | NONE — ws:// and http:// only (localhost dev only) |
| Hardcoded credentials | NONE found |
| Input sanitization | JSON parsing uses `QJsonDocument::fromJson` + null checks in EnvelopeCodec |
| Binary frame validation | BinaryCodec returns empty payload if frame < 4 bytes |
| ONNX model integrity | SHA-256 verified before use |
| Microphone permission | QPermission API (macOS) — proper platform permission flow |
| Device credentials | UUID + token stored in QSettings (macOS Keychain not used) |

**Risk:** Plain-text WebSocket (ws://) and HTTP log forwarding acceptable only for localhost. Production deployment requires TLS upgrade.

---
