# pairion-client — Codebase Audit

**Audit Date:** 2026-04-19T17:30:00Z
**Branch:** main
**Commit:** 3905e308801c11646fb1c79166b280fcd29bdc4a chore: remove all GitHub CI references
**Auditor:** Claude Code (Automated)
**Purpose:** Zero-context reference for AI-assisted development
**Audit File:** pairion-client-Audit.md
**Scorecard:** pairion-client-Scorecard.md
**OpenAPI Spec:** N/A (WebSocket/AsyncAPI protocol — no REST API; use Architecture.md for protocol reference)

> This audit is the source of truth for the pairion-client codebase structure, modules, services, and configuration.
> An AI reading this audit should be able to generate accurate code changes, new features, tests, and fixes without filesystem access.

---
## 1. Project Identity

```
Project Name:        Pairion Client (pairion-client)
Repository URL:      ~/Documents/GitHub/Pairion-Client
Primary Language:    C++20
Framework:           Qt 6.5+ (Core, Quick, Network, WebSockets, Multimedia, Test)
Build Tool:          CMake 3.25+ with Ninja generator; CMakePresets.json for platform variants
C++ Standard:        C++20 (CMAKE_CXX_STANDARD=20, REQUIRED)
Current Branch:      main
Latest Commit Hash:  3905e308801c11646fb1c79166b280fcd29bdc4a
Latest Commit Msg:   chore: remove all GitHub CI references
Audit Timestamp:     2026-04-19T17:30:00Z
App Version:         0.2.0 (CMakeLists.txt project version); kClientVersion = "0.1.0"
```

## 2. Directory Structure

```
pairion-client/
├── CMakeLists.txt                           ← Build definition; defines pairion_core lib + pairion executable + all tests
├── CMakePresets.json                        ← Platform presets (macOS arm64/x86, Linux x86/arm, Windows)
├── resources.qrc                            ← Qt resource bundle: QML files + model_manifest.json
├── Architecture.md                         ← Architecture reference doc
├── CHANGELOG.md
├── CONVENTIONS.md
├── README.md
├── cmake/
│   └── check_coverage.cmake                ← LCOV coverage threshold enforcer (100% required)
├── qml/
│   ├── Main.qml                            ← Root ApplicationWindow (480×640, dark background)
│   └── Debug/
│       └── DebugPanel.qml                  ← M0/M1 debug panel bound to ConnectionState + Settings QML singletons
├── resources/
│   └── model_manifest.json                 ← 4-model ONNX manifest (melspectrogram, embedding, hey_jarvis, silero_vad)
├── src/
│   ├── main.cpp                            ← Entry point; startup sequence + pipeline wiring
│   ├── audio/
│   │   ├── pairion_audio_capture.{h,cpp}   ← QAudioSource capture, 20ms PCM frame emission
│   │   ├── pairion_audio_playback.{h,cpp}  ← QAudioSink playback with Opus decoder + jitter buffer
│   │   ├── pairion_opus_encoder.{h,cpp}    ← libopus encoder (28kbps VOIP, worker thread)
│   │   ├── pairion_opus_decoder.{h,cpp}    ← libopus decoder (16kHz mono)
│   │   ├── ring_buffer.h                   ← Lock-free SPSC ring buffer (header-only template)
│   │   └── README.md
│   ├── core/
│   │   ├── constants.h                     ← Compile-time constants (URLs, timings, frame sizes)
│   │   ├── device_identity.{h,cpp}         ← UUID device ID + bearer token via QSettings
│   │   ├── model_downloader.{h,cpp}        ← ONNX model download with SHA-256 verification
│   │   ├── onnx_session.{h,cpp}            ← Abstract OnnxInferenceSession + OrtInferenceSession impl
│   │   └── README.md
│   ├── hud/
│   │   └── README.md                       ← Placeholder for future HUD implementation
│   ├── pipeline/
│   │   ├── audio_session_orchestrator.{h,cpp} ← Central pipeline state machine (Idle→AwaitingWake→Streaming→EndingSpeech)
│   │   └── README.md
│   ├── protocol/
│   │   ├── messages.h                      ← All protocol message structs + OutboundMessage/InboundMessage variants
│   │   ├── envelope_codec.{h,cpp}          ← JSON text-frame serializer/deserializer
│   │   └── binary_codec.{h,cpp}            ← Binary frame encoder (4-byte stream ID prefix + Opus payload)
│   ├── settings/
│   │   ├── settings.{h,cpp}                ← QSettings-backed singleton exposed to QML
│   │   └── README.md
│   ├── state/
│   │   └── connection_state.{h,cpp}        ← QML singleton tracking WebSocket state + transcript + LLM response
│   ├── util/
│   │   └── logger.{h,cpp}                  ← Centralized logger; installs Qt message handler; batches to POST /v1/logs
│   ├── vad/
│   │   ├── silero_vad.{h,cpp}              ← Silero VAD v5 ONNX wrapper; state machine Idle→Speaking→Trailing
│   │   └── README.md
│   ├── wake/
│   │   ├── open_wakeword_detector.{h,cpp}  ← Three-model openWakeWord pipeline (mel→embedding→classifier)
│   │   └── README.md
│   └── ws/
│       ├── pairion_websocket_client.{h,cpp} ← QWebSocket client; heartbeat; exponential backoff reconnect
└── test/
    ├── mock_onnx_session.h                  ← Mock OnnxInferenceSession returning canned outputs
    ├── mock_server.{h,cpp}                  ← QWebSocketServer mock implementing Pairion AsyncAPI protocol
    ├── tst_audio_session_orchestrator.cpp
    ├── tst_binary_frame.cpp
    ├── tst_device_identity.cpp
    ├── tst_integration.cpp
    ├── tst_log_batching.cpp
    ├── tst_message_codec.cpp
    ├── tst_model_downloader.cpp
    ├── tst_opus_codec.cpp
    ├── tst_ring_buffer.cpp
    ├── tst_settings.cpp
    ├── tst_silero_vad.cpp
    ├── tst_wake_detector.cpp
    └── tst_ws_client_state_machine.cpp
```

Single-module C++/Qt desktop application. All production source lives in `src/` organized by subsystem. Tests are in `test/` as separate Qt Test executables, each linked against the `pairion_core` static library.

## 3. Build & Dependency Manifest

**Build file:** `CMakeLists.txt`

### Library Targets

| Target | Type | Sources |
|---|---|---|
| `pairion_core` | STATIC lib | All `src/**/*.cpp` except `main.cpp` |
| `pairion` | Executable | `src/main.cpp` + `resources.qrc`, links `pairion_core` |

### External Dependencies

| Dependency | Version | Purpose |
|---|---|---|
| Qt6::Core | 6.5+ | Core Qt types, QObject, signals/slots, QSettings, QTimer, QThread, QUuid |
| Qt6::Quick | 6.5+ | QML engine, `qmlRegisterSingletonInstance` |
| Qt6::Network | 6.5+ | QNetworkAccessManager, QNetworkReply (HTTP log forwarding, model download) |
| Qt6::WebSockets | 6.5+ | QWebSocket, QWebSocketServer (in tests) |
| Qt6::Multimedia | 6.5+ | QAudioSource, QAudioSink, QMediaDevices, QAudioFormat |
| Qt6::Test | 6.5+ | QTest framework (test targets only) |
| opus (via pkg-config) | system | libopus — audio encoding/decoding (VOIP 28kbps) |
| onnxruntime | system | ONNX Runtime C++ API — mel/embedding/classifier/VAD model inference |

### Build Commands

```
Configure (macOS ARM64 debug):
  cmake --preset macos-arm64-debug

Build:
  cmake --build --preset macos-arm64-debug

Test:
  ctest --preset macos-arm64-debug

Run with coverage:
  cmake --build --preset macos-arm64-debug --target coverage
  (requires PAIRION_COVERAGE=ON in preset; uses lcov + genhtml; enforces 100% threshold)

Run app:
  ./build/macos-arm64-debug/pairion.app/Contents/MacOS/pairion
```

### Compiler Flags

Debug presets: `-Wall -Wextra -Wpedantic -Werror` (all warnings as errors)
Coverage: `--coverage` on `pairion_core` when `PAIRION_COVERAGE=ON`

### CMake Options

| Option | Default | Effect |
|---|---|---|
| `PAIRION_COVERAGE` | OFF | Enables `--coverage` instrumentation and `coverage` target |
| `PAIRION_CLIENT_NATIVE_TESTS` | OFF | Enables tests requiring real ONNX model files on disk |
| `PAIRION_COVERAGE_THRESHOLD` | 100 | Minimum line coverage % (enforced by `cmake/check_coverage.cmake`) |

## 4. Configuration & Infrastructure Summary

This is a client-only application with no server-side configuration, no database, and no Docker/CI config.

### Runtime Configuration

**QSettings** (system-native: `~/Library/Preferences/Pairion.pairion.plist` on macOS):

| Key | Type | Default | Used By |
|---|---|---|---|
| `device/id` | QString | auto-generated UUID | DeviceIdentity → WebSocket DeviceIdentify |
| `device/bearerToken` | QString | auto-generated UUID | DeviceIdentity → WebSocket DeviceIdentify |
| `wake/threshold` | double | 0.3 | Settings → OpenWakewordDetector |
| `vad/silence_end_ms` | int | 800 | Settings → SileroVad |
| `vad/threshold` | double | 0.5 | Settings → SileroVad |
| `audio/input_device` | QString | empty (system default) | Settings (exposed to QML; not yet wired to QAudioSource) |
| `audio/sample_rate` | int | 16000 | Settings (exposed to QML; QAudioSource hardcodes 16kHz) |

### Compile-Time Constants (`src/core/constants.h`)

| Constant | Value | Purpose |
|---|---|---|
| `kClientVersion` | "0.1.0" | Sent in DeviceIdentify; kClientVersion ≠ project version (0.2.0) |
| `kDefaultServerUrl` | "ws://localhost:18789/ws/v1" | WebSocket server URL |
| `kDefaultRestBaseUrl` | "http://localhost:18789/v1" | REST log forwarding base URL |
| `kHeartbeatIntervalMs` | 15000 | Heartbeat ping interval |
| `kHeartbeatDeadlineMs` | 10000 | Heartbeat pong deadline (not yet enforced in code) |
| `kLogFlushIntervalMs` | 30000 | Logger flush timer interval |
| `kReconnectBackoffMs[]` | {1000,2000,4000,8000,15000,30000} | Exponential backoff steps |
| `kReconnectBackoffSteps` | 6 | Backoff array length |
| `kPcmFrameBytes` | 640 | 20ms PCM frame (320 samples × 2 bytes) |
| `kPreRollBytes` | 6400 | ~200ms pre-roll buffer |
| `kStreamingTimeoutMs` | 30000 | Safety cap for streaming sessions |
| `kWakeSuppresssionMs` | 500 | False-wake suppression window (note: typo in constant name: "Suppresssion") |

### Connection Map

```
WebSocket Server:  ws://localhost:18789/ws/v1 (hardcoded in constants.h)
REST Log Endpoint: http://localhost:18789/v1/logs (POST, batched every 30s)
ONNX Models:       downloaded to QStandardPaths::AppDataLocation/models/ on first launch
Model Sources:     github.com/dscripka/openWakeWord/releases/download/v0.5.1/ (3 models)
                   github.com/snakers4/silero-vad/raw/v5.1.2/ (1 model)
Database:          None
Cache:             None (model files cached to AppDataLocation/models/)
Message Broker:    None
Cloud Services:    None
```

### CI/CD

No CI/CD pipeline detected (GitHub workflows removed per latest commit).

## 5. Startup & Runtime Behavior

**Entry Point:** `src/main.cpp :: main()`

### Startup Sequence

1. `QGuiApplication` constructed
2. `QNetworkAccessManager`, `ConnectionState`, `Settings` instantiated
3. `Logger` instantiated, `setLogCallback` → `connState->appendLog`, `logger->install()` (installs Qt message handler, starts 30s flush timer)
4. `DeviceIdentity` instantiated (loads or generates UUID + bearer token from QSettings)
5. `PairionWebSocketClient` instantiated (serverUrl, deviceId, bearerToken, connState)
6. `PairionAudioCapture` and `PairionOpusEncoder` instantiated; encoder moved to `EncoderThread`
7. `InferenceThread` created (for wake detector + VAD)
8. QML singletons registered: `ConnectionState` and `Settings` as `Pairion 1.0`
9. `QQmlApplicationEngine` loads `qrc:/qml/Main.qml`
10. `ModelDownloader` instantiated (downloads ONNX models on `allModelsReady` signal from wsClient)
11. Microphone permission checked via `QMicrophonePermission` API (macOS/Qt permission system)
12. `wsClient->connectToServer()` — starts WebSocket connection immediately
13. On `sessionOpened` → `modelDownloader->checkAndDownload()`
14. On **BOTH** mic permission granted AND all models ready → `initAudioPipeline()` called

### `initAudioPipeline()` Sequence

1. Loads 4 ONNX model files from AppDataLocation/models/
2. Creates `OpenWakewordDetector` + `SileroVad` + `PairionAudioPlayback`
3. Moves wake detector and VAD to `InferenceThread`
4. Creates `AudioSessionOrchestrator` (wires all signal connections)
5. Connects capture → encoder, capture → wake detector, capture → VAD (all queued cross-thread)
6. Starts `InferenceThread` and `EncoderThread`
7. Warms up wake detector (silence frames) on inference thread
8. Calls `capture->start()` and `orchestrator->startListening()`

### Threading Model

| Thread | Objects | Notes |
|---|---|---|
| Main thread | QML engine, PairionAudioCapture, ConnectionState, PairionWebSocketClient, AudioSessionOrchestrator, PairionAudioPlayback | QAudioSource requires main thread on macOS |
| EncoderThread | PairionOpusEncoder | CPU-bound Opus encoding |
| InferenceThread | OpenWakewordDetector, SileroVad | ONNX inference (~ms per frame at 50fps) |

### Shutdown Sequence

`app.exec()` returns → `inferenceThread->quit()` + `wait()` → `encoderThread->quit()` + `wait()` → `delete encoder`

### Health Endpoints

None (client-only application, no HTTP server).

## 6. Module Reference (C++ Classes)

This section documents every class, its public interface, and key implementation notes.

---

### `pairion::core::DeviceIdentity` — `src/core/device_identity.{h,cpp}`

**Purpose:** Loads or generates device credentials from QSettings.

**Constructors:**
- `DeviceIdentity(QObject *parent)` — loads from QSettings, generates UUIDs if absent
- `DeviceIdentity(const QString &deviceId, const QString &bearerToken, QObject *parent)` — explicit credentials for tests

**Public Methods:**
- `const QString &deviceId() const` — device UUID (QSettings key: `device/id`)
- `const QString &bearerToken() const` — bearer token (QSettings key: `device/bearerToken`)

**Private:**
- `loadOrCreate()` — reads/creates UUIDs via `QUuid::createUuid()` with `WithoutBraces` format

---

### `pairion::core::OnnxInferenceSession` — `src/core/onnx_session.h`

**Purpose:** Abstract testability seam for ONNX model inference.

```cpp
virtual std::vector<OnnxOutput> run(
    const std::vector<OnnxTensor> &inputs,
    const std::vector<std::string> &outputNames) = 0;
```

**Types:**
- `OnnxTensor { name, data (float32), int64Data, shape }` — input tensor; `isInt64()` returns true when int64Data non-empty
- `OnnxOutput { data (float32), shape }` — output tensor

**Implementations:**
- `OrtInferenceSession` (production) — wraps `Ort::Session`; loads model from path; intraOpThreads=1, interOpThreads=1, ORT_ENABLE_ALL optimization
- `MockOnnxSession` (tests, `test/mock_onnx_session.h`) — queue-based canned outputs; tracks `runCount()` and `lastInputs()`

---

### `pairion::core::ModelDownloader` — `src/core/model_downloader.{h,cpp}`

**Purpose:** Downloads and SHA-256 verifies ONNX model files, caches to AppDataLocation/models/.

**Public Methods:**
- `void checkAndDownload()` — reads manifest, checks cache, downloads missing/corrupt models
- `static QString modelCacheDir()` — `QStandardPaths::AppDataLocation + "/models"`
- `static QString modelPath(const QString &name)` — `modelCacheDir() + "/" + name`

**Signals:**
- `downloadProgress(int percent)` — 0-100 overall progress
- `allModelsReady()` — all 4 models verified
- `downloadError(const QString &reason)` — download or hash failure

**Manifest:** `:/resources/model_manifest.json` — array of `{name, url, sha256}`

**Models (from manifest):**
- `melspectrogram.onnx` — sha256: ba2b0e0f…
- `embedding_model.onnx` — sha256: 70d16429…
- `hey_jarvis_v0.1.onnx` — sha256: 94a13cfe…
- `silero_vad.onnx` — sha256: 2623a295…

---

### `pairion::settings::Settings` — `src/settings/settings.{h,cpp}`

**Purpose:** QSettings-backed QObject exposed to QML as singleton.

**Q_PROPERTYs (all READ/WRITE/NOTIFY):**
- `wakeThreshold` (double, default 0.3)
- `vadSilenceEndMs` (int, default 800)
- `vadThreshold` (double, default 0.5)
- `audioInputDevice` (QString, default empty)
- `audioSampleRate` (int, default 16000)

All setters write immediately to QSettings.

---

### `pairion::state::ConnectionState` — `src/state/connection_state.{h,cpp}`

**Purpose:** QML singleton tracking WebSocket connection state and pipeline display data.

**Status enum:** `Disconnected=0, Connecting=1, Connected=2, Reconnecting=3` (Q_ENUM for QML)

**Q_PROPERTYs (all READ/NOTIFY):**
- `status` (Status enum)
- `sessionId` (QString)
- `serverVersion` (QString)
- `reconnectAttempts` (int)
- `recentLogs` (QStringList, max 50 entries, prepend order)
- `transcriptPartial` (QString)
- `transcriptFinal` (QString)
- `llmResponse` (QString, accumulated from token deltas)
- `agentState` (QString: idle/thinking/speaking)
- `voiceState` (QString: idle/awaiting_wake/streaming/ending_speech)

**Slots:** `setStatus`, `setSessionId`, `setServerVersion`, `setReconnectAttempts`, `appendLog`, `setTranscriptPartial`, `setTranscriptFinal`, `setAgentState`, `setVoiceState`, `appendLlmToken`, `clearLlmResponse`

---

### `pairion::util::Logger` — `src/util/logger.{h,cpp}`

**Purpose:** Centralized logger; installs Qt message handler; batches log records; POSTs to `POST /v1/logs`.

**Public Methods:**
- `void install()` — calls `qInstallMessageHandler(Logger::messageHandler)`, starts flush timer
- `void setSessionId(const QString &)` — thread-safe (mutex)
- `void setLogCallback(std::function<void(const QString&)>)` — callback for debug panel display
- `void flush()` — immediate HTTP POST of buffered records; thread-safe
- `QJsonArray pendingRecords() const` — for testing
- `int pendingCount() const` — for testing

**Static:**
- `messageHandler(QtMsgType, context, msg)` — installed Qt message handler; calls `enqueue()` + callback

**Level mapping:** DEBUG→DEBUG, INFO→INFO, WARNING→WARN, CRITICAL→ERROR, FATAL→ERROR (unreachable after fatal)

**Log record JSON:** `{timestamp (ISO8601+ms), level, message, sessionId?}`

**Timer:** 30-second flush interval (`kLogFlushIntervalMs`)

---

### `pairion::ws::PairionWebSocketClient` — `src/ws/pairion_websocket_client.{h,cpp}`

**Purpose:** WebSocket client implementing Pairion AsyncAPI protocol lifecycle.

**Public Methods:**
- `void connectToServer()` — opens WebSocket, sets status=Connecting
- `void disconnectFromServer()` — intentional close, stops heartbeat + reconnect
- `void sendMessage(const OutboundMessage&)` — serializes via EnvelopeCodec, sends text frame
- `bool isConnected() const` — true when `QAbstractSocket::ConnectedState`
- `void setHeartbeatIntervalMs(int)` — override for testing
- `void sendBinaryFrame(const QByteArray&)` — sends raw binary frame

**Key Signals:** `sessionOpened`, `sessionClosed`, `serverError`, `disconnected`, `inboundMessage`, `binaryFrameReceived`, `transcriptPartialReceived`, `transcriptFinalReceived`, `agentStateReceived`, `llmTokenReceived`

**On Connect:** Sends `DeviceIdentify`, starts heartbeat (15s)
**On Disconnect:** Schedules reconnect with exponential backoff (1s→2s→4s→8s→15s→30s, 6 steps)
**Inbound dispatch:** `std::visit` on `InboundMessage` variant; dispatches to ConnectionState setters and typed signals

---

### `pairion::protocol::EnvelopeCodec` — `src/protocol/envelope_codec.{h,cpp}`

**Purpose:** Stateless JSON text-frame codec. Thread-safe.

**Methods:**
- `static QJsonObject serialize(const OutboundMessage&)` — std::visit dispatch per message type
- `static QString serializeToString(const OutboundMessage&)` — compact JSON string
- `static std::optional<InboundMessage> deserialize(const QJsonObject&)` — type-discriminated by `type` field
- `static std::optional<InboundMessage> deserializeFromString(const QString&)` — parses JSON then calls deserialize

**Discriminator:** `type` field in JSON matches struct `kType` constants.
Returns `std::nullopt` for unknown message types.

---

### `pairion::protocol::BinaryCodec` — `src/protocol/binary_codec.{h,cpp}`

**Purpose:** Binary frame assembly: 4-byte stream ID prefix + Opus payload.

**Methods:**
- `static QByteArray encodeBinaryFrame(const QString &streamId, const QByteArray &opusPayload)`
- `static DecodedBinaryFrame decodeBinaryFrame(const QByteArray &frame)` — frame.size() >= 4 required
- `static QByteArray streamIdToPrefix(const QString &streamId)` — `QUuid::fromString()` → `toRfc4122()` → first 4 bytes

**Prefix:** RFC 4122 time_low field (4 bytes, big-endian) from stream UUID.

---

### `pairion::audio::PairionAudioCapture` — `src/audio/pairion_audio_capture.{h,cpp}`

**Purpose:** QAudioSource wrapper emitting exact 20ms PCM frames (640 bytes).

**Constructors:**
- `PairionAudioCapture(QObject*)` — uses system default microphone
- `PairionAudioCapture(QIODevice*, QObject*)` — for testing with external device

**Format:** 16kHz, mono, Int16 signed PCM
**Slots:** `start()`, `stop()`
**Signal:** `audioFrameAvailable(const QByteArray &pcm20ms)` — always exactly 640 bytes
**Threading:** Must live on main thread (macOS AVFoundation requirement)
**Accumulator:** `m_accumulator` — internal QByteArray accumulating variable-size QAudioSource pushes until 640 bytes available

---

### `pairion::audio::PairionOpusEncoder` — `src/audio/pairion_opus_encoder.{h,cpp}`

**Purpose:** libopus encoder at 28kbps VOIP for 20ms PCM frames; worker thread.

**Config:** 16kHz, mono, OPUS_APPLICATION_VOIP, 28000bps, complexity=5, OPUS_SIGNAL_VOICE
**Slot:** `encodePcmFrame(const QByteArray &pcm20ms)` — must be exactly 640 bytes
**Signals:** `opusFrameEncoded(const QByteArray&)`, `encoderError(const QString&)`
**Method:** `bool isValid() const`

---

### `pairion::audio::PairionOpusDecoder` — `src/audio/pairion_opus_decoder.{h,cpp}`

**Purpose:** libopus decoder producing 16kHz mono PCM from Opus frames.

**Slot:** `decodeOpusFrame(const QByteArray &opusFrame)`
**Signals:** `pcmFrameDecoded(const QByteArray &pcm)`, `decoderError(const QString&)`
**Method:** `bool isValid() const`
**Max frame:** 5760 samples (120ms at 48kHz, Opus maximum)

---

### `pairion::audio::PairionAudioPlayback` — `src/audio/pairion_audio_playback.{h,cpp}`

**Purpose:** QAudioSink playback with Opus decoder and jitter buffer for inbound server audio.

**Slots:** `handleOpusFrame(const QByteArray&)`, `handlePcmFrame(const QByteArray&)`, `handleStreamEnd(const QString&)`
**Signal:** `playbackError(const QString&)`, `speakingStateChanged(const QString&)` — "speaking" / "idle"
**Jitter buffer:** Simple QQueue of PCM chunks, flushed synchronously to QAudioSink
**Silence timer:** 500ms one-shot; emits speakingStateChanged("idle") after last PCM

**NOTE:** `pairion_audio_playback.cpp` is missing the `@file` doc comment and class-level Doxygen comment.

---

### `pairion::audio::RingBuffer<T, Capacity>` — `src/audio/ring_buffer.h`

**Purpose:** Lock-free SPSC ring buffer; header-only template.

**Constraint:** Capacity must be power of two (static_assert enforced).
**Memory ordering:** Producer: release store on writeIdx; Consumer: acquire load on writeIdx.
**Methods:** `push(const T&) → bool`, `pop(T&) → bool`, `isEmpty()`, `isFull()`, `size()`

---

### `pairion::wake::OpenWakewordDetector` — `src/wake/open_wakeword_detector.{h,cpp}`

**Purpose:** Three-model openWakeWord ONNX pipeline (mel → embedding → classifier).

**Constructor:** Injects 3 OnnxInferenceSession pointers + threshold (default 0.5)
**Slots:** `warmup()`, `processPcmFrame(const QByteArray &pcm20ms)`
**Signal:** `wakeWordDetected(float score, const QByteArray &preRollBuffer)`

**Pipeline stages:**
1. Accumulate PCM to 1280 samples (80ms chunks)
2. Mel: sliding window of last 1760 samples (1280+480 context) → [1, 1760] float32 → melspectrogram output
   - Scale transform: `v / 10.0f + 2.0f` (matches openWakeWord Python reference)
   - Takes only LAST 8 mel rows per chunk (avoids context-row duplication)
   - Rolling buffer max 200 rows
3. Embedding: last 76 mel rows → [1, 76, 32, 1] → `conv2d_19` output (96 floats)
   - Rolling buffer max 120 features
4. Classifier: last 16 features → [1, 16, 96] → `53` output (score float)
5. Score written to `/tmp/pairion_wake_scores.csv` (diagnostic file logging)
6. Threshold check + 500ms suppression window

**Pre-roll:** Rolling buffer of last 6400 bytes (~200ms) raw PCM

**CRITICAL OBSERVATION:** Wake detector writes scores to `/tmp/pairion_wake_scores.csv` on every inference cycle using a static QFile — this is debug instrumentation that should be removed before production.

---

### `pairion::vad::SileroVad` — `src/vad/silero_vad.{h,cpp}`

**Purpose:** Silero VAD v5 ONNX wrapper with speech/silence state machine.

**Constructor:** Injects OnnxInferenceSession + threshold (default 0.5) + silenceEndMs (default 800)
**Slots:** `processPcmFrame(const QByteArray &pcm20ms)`, `reset()`
**Signals:** `speechStarted()`, `speechEnded()`

**Input format:** Accumulates 20ms frames to 512-sample chunks; normalizes to [-1,1] (÷32768.0f)
**ONNX inputs:** `input [1,512]`, `state [2,1,128]`, `sr {16000}` (int64)
**ONNX outputs:** `output [1,1]`, `stateN [2,1,128]`

**State machine:** Idle → Speaking (prob ≥ threshold) → Trailing (prob < threshold) → Idle (silence ≥ silenceEndMs)

---

### `pairion::pipeline::AudioSessionOrchestrator` — `src/pipeline/audio_session_orchestrator.{h,cpp}`

**Purpose:** Central state machine coordinating the full audio pipeline.

**States:** `Idle, AwaitingWake, Streaming, EndingSpeech`

**Constructor:** Injects all 7 pipeline objects (capture, encoder, wakeDetector, vad, wsClient, connState, playback)
**Slots (public):** `startListening()` — transitions Idle→AwaitingWake; `shutdown()` — stops all activity
**Signals:** `wakeFired(streamId)`, `streamEnded(streamId)`, `pipelineError(message)`

**State transitions:**
- AwaitingWake + `onWakeWordDetected` → Streaming: sends WakeWordDetected + AudioStreamStart envelopes; sends pre-roll PCM as binary frames; starts 30s timeout timer; resets VAD
- Streaming + `onSpeechEnded` → Idle: sends SpeechEnded + AudioStreamEnd envelopes; auto-restarts listening
- Streaming + `onStreamingTimeout` → Idle (same as above, reason="timeout")
- Streaming + `onOpusFrameEncoded` → sends binary frame via `BinaryCodec::encodeBinaryFrame`

**CRITICAL TODO:** `audio_session_orchestrator.cpp:89` — pre-roll audio is NOT encoded through Opus before sending. Raw PCM is iterated but the send is never called — pre-roll frames are currently dropped.

**Inbound audio:** `onInboundAudio(opusFrame)` → `playback->handleOpusFrame()` + sets agentState="speaking"
**Stream voiceState values:** idle, awaiting_wake, streaming, ending_speech (set via `transitionTo`)

## 7. Protocol Message Types

### Outbound Messages (Client → Server)

Variant type: `pairion::protocol::OutboundMessage`

| Struct | `kType` | Fields |
|---|---|---|
| `DeviceIdentify` | "DeviceIdentify" | deviceId, bearerToken, clientVersion |
| `HeartbeatPing` | "HeartbeatPing" | timestamp |
| `WakeWordDetected` | "WakeWordDetected" | timestamp, confidence (optional) |
| `AudioStreamStartIn` | "AudioStreamStart" | streamId, codec="opus", sampleRate=16000 |
| `SpeechEnded` | "SpeechEnded" | streamId |
| `AudioStreamEndIn` | "AudioStreamEnd" | streamId, reason |
| `TextMessage` | "TextMessage" | text |

### Inbound Messages (Server → Client)

Variant type: `pairion::protocol::InboundMessage`

| Struct | `kType` | Fields |
|---|---|---|
| `SessionOpened` | "SessionOpened" | sessionId, serverVersion |
| `SessionClosed` | "SessionClosed" | reason |
| `HeartbeatPong` | "HeartbeatPong" | timestamp |
| `ErrorMessage` | "Error" | code, message |
| `AgentStateChange` | "AgentStateChange" | state (idle/thinking/speaking) |
| `TranscriptPartial` | "TranscriptPartial" | text |
| `TranscriptFinal` | "TranscriptFinal" | text |
| `LlmTokenStream` | "LlmTokenStream" | delta |
| `ToolCallStarted` | "ToolCallStarted" | toolCallId, toolName, input (QJsonObject) |
| `ToolCallCompleted` | "ToolCallCompleted" | toolCallId, output (QJsonObject) |
| `AudioStreamStartOut` | "AudioStreamStart" | streamId, codec, sampleRate |
| `AudioStreamEndOut` | "AudioStreamEnd" | streamId, reason |
| `UnderBreathAck` | "UnderBreathAck" | acknowledgementType (optional) |

**Note:** `AudioStreamStart` kType is shared between `AudioStreamStartIn` (outbound) and `AudioStreamStartOut` (inbound). Disambiguation is by direction.

## 8. QML Layer

### Main.qml (`qml/Main.qml`)

- `ApplicationWindow` root: 480×640, dark background (#1a1a2e), title "Pairion — Debug"
- Instantiates `DebugPanel` filling the window with 16px margins
- Imports: `QtQuick`, `QtQuick.Controls`, `"Debug"` (relative import for DebugPanel.qml)

### DebugPanel.qml (`qml/Debug/DebugPanel.qml`)

- Imports: `Pairion 1.0` — binds to `ConnectionState` and `Settings` QML singletons
- Sections: connection status (colored dot), session/server/reconnect info, voice pipeline state, agent state, transcript (partial + final), LLM response (scrollable, 80px), recent logs (selectable TextArea, full-height, joins array with \n)
- Status dot colors: Disconnected=#ff4444, Connecting/Reconnecting=#ffaa00, Connected=#44ff44
- VoiceState colors: awaiting_wake=#5599ff, streaming=#44ff44, ending_speech=#ffaa00, else=#888888

## 9. Test Suite

**Framework:** Qt Test (QTest) — each test is a separate executable linked to `pairion_core`.
**Runner:** `ctest` via CMake test presets.

### Test Executables

| Executable | Source File | Covers |
|---|---|---|
| `tst_message_codec` | `tst_message_codec.cpp` | EnvelopeCodec: full round-trip serialize/deserialize for every message type |
| `tst_log_batching` | `tst_log_batching.cpp` | Logger: install, buffer, flush, pendingCount |
| `tst_ws_client_state_machine` | `tst_ws_client_state_machine.cpp` | PairionWebSocketClient state machine with MockServer |
| `tst_device_identity` | `tst_device_identity.cpp` | DeviceIdentity: QSettings persistence, explicit constructor |
| `tst_integration` | `tst_integration.cpp` | End-to-end pipeline with MockServer |
| `tst_binary_frame` | `tst_binary_frame.cpp` | BinaryCodec: encode/decode, streamIdToPrefix, edge cases |
| `tst_settings` | `tst_settings.cpp` | Settings: QSettings round-trip for all properties |
| `tst_ring_buffer` | `tst_ring_buffer.cpp` | RingBuffer: push/pop, full/empty, SPSC semantics |
| `tst_opus_codec` | `tst_opus_codec.cpp` | PairionOpusEncoder + PairionOpusDecoder: round-trip PCM→Opus→PCM |
| `tst_wake_detector` | `tst_wake_detector.cpp` | OpenWakewordDetector with MockOnnxSession |
| `tst_silero_vad` | `tst_silero_vad.cpp` | SileroVad state machine with MockOnnxSession |
| `tst_model_downloader` | `tst_model_downloader.cpp` | ModelDownloader: HTTP download, SHA-256 verification, error cases |
| `tst_audio_session_orchestrator` | `tst_audio_session_orchestrator.cpp` | AudioSessionOrchestrator state machine with MockServer |

### Test Infrastructure

- **`MockOnnxSession`** (`test/mock_onnx_session.h`): queue-based canned ONNX outputs; tracks `runCount()` and `lastInputs()`
- **`MockServer`** (`test/mock_server.{h,cpp}`): `QWebSocketServer` implementing Pairion AsyncAPI protocol; responds to DeviceIdentify → SessionOpened; echoes heartbeats; can inject arbitrary messages

### Coverage

Coverage measured via lcov when `PAIRION_COVERAGE=ON`. Threshold: 100% line coverage enforced by `cmake/check_coverage.cmake`. Coverage instrumentation on `pairion_core` only (main.cpp, QML, generated MOC files excluded via lcov ignore patterns).

## 10. Logging Categories

All logging uses Qt's `Q_LOGGING_CATEGORY` mechanism with `qCInfo`/`qCDebug`/`qCWarning`/`qCCritical`.
The `Logger` class installs a custom message handler that intercepts all `qDebug`/etc. output and batches to POST /v1/logs.

| Category | File | Description |
|---|---|---|
| `pairion.onnx` | onnx_session.cpp | ONNX model load and inference |
| `pairion.model` | model_downloader.cpp | Model download and verification |
| `pairion.audio.capture` | pairion_audio_capture.cpp | Microphone capture start/stop |
| `pairion.audio.opus.enc` | pairion_opus_encoder.cpp | Opus encoding |
| `pairion.audio.opus.dec` | pairion_opus_decoder.cpp | Opus decoding |
| `pairion.pipeline` | audio_session_orchestrator.cpp | Pipeline state machine transitions |
| `pairion.protocol.binary` | binary_codec.cpp | Binary frame encode/decode |
| `pairion.vad` | silero_vad.cpp | VAD inference and state changes |
| `pairion.wake` | open_wakeword_detector.cpp | Wake word pipeline and detection |
| `pairion.ws` | pairion_websocket_client.cpp | WebSocket connection lifecycle |

## 11. Security Notes

**Authentication:** Device bearer token (UUID, persisted in QSettings). Sent in DeviceIdentify on every WebSocket connection.

**No password handling:** No user login, no password storage.

**Token storage:** Bearer token stored in `QSettings` (system keychain/plist on macOS). Not encrypted at application level.

**Communication:** WebSocket over plain `ws://` (not `wss://`). No TLS in current configuration. Production hardening required.

**Model integrity:** SHA-256 hash verification on every ONNX model file (both at download time and cache-hit time). Empty hash causes rejection.

**Input validation:** PCM frame size validated before Opus encoding (must be exactly 640 bytes). Binary frames validated to be >= 4 bytes before decode. JSON parse errors return `std::nullopt` (not crashes).

**No authentication on REST logs endpoint:** `POST /v1/logs` has no auth header — relies on localhost-only deployment for security.

**CORS/CSRF:** N/A (not a web application).

**Rate limiting:** None implemented.

## 12. Service Dependencies

```
This client → Pairion Server
  WebSocket: ws://localhost:18789/ws/v1 (AsyncAPI protocol)
  REST:      http://localhost:18789/v1/logs (POST, log batching)

External (download time only):
  github.com/dscripka/openWakeWord — ONNX model files
  github.com/snakers4/silero-vad   — ONNX model file

Downstream consumers: None (client application)
```

## 13. Known Technical Debt & Issues

**TODO/STUB Scan result:** 1 hit found — CRITICAL

| Issue | Location | Severity | Notes |
|---|---|---|---|
| Pre-roll not Opus-encoded before send | `audio_session_orchestrator.cpp:89` | CRITICAL | `TODO(PC-003)`: loop iterates pre-roll PCM frames but never sends them — frames are silently dropped. Pre-roll should be encoded via Opus before transmission. |
| Debug file logging in production code | `open_wakeword_detector.cpp:191-198` | HIGH | Static `QFile` writes scores to `/tmp/pairion_wake_scores.csv` on every classifier inference. Must be removed before production. |
| kClientVersion ≠ project version | `constants.h:18` | LOW | `kClientVersion = "0.1.0"` but `CMakeLists.txt project VERSION = 0.2.0`. |
| kHeartbeatDeadlineMs unused | `constants.h:32` | LOW | Constant defined (10000ms) but pong deadline is never enforced in the WebSocket client. |
| `kWakeSuppresssionMs` typo | `constants.h:52` | LOW | Triple 's' in constant name; not referenced externally but inconsistent. |
| `audioInputDevice` / `audioSampleRate` settings not wired | `settings.{h,cpp}`, `pairion_audio_capture.cpp` | MEDIUM | QML-exposed settings exist but `PairionAudioCapture` always uses system default device and hardcodes 16kHz. |
| PairionAudioPlayback missing class Doxygen | `pairion_audio_playback.h:18` | LOW | Class declaration lacks `/** @brief */` comment; constructor missing doc; `start()`/`stop()` undocumented. |
| No TLS on WebSocket | `constants.h:22` | MEDIUM | `ws://` scheme — plaintext connection. Must become `wss://` for production. |
| No auth on log POST endpoint | `logger.cpp:71` | MEDIUM | HTTP POST to `/v1/logs` has no Authorization header. Relies on localhost deployment only. |
| HUD src/hud/ is empty | `src/hud/README.md` | INFO | Placeholder only. No implementation. |

## 14. Security Vulnerability Scan (Snyk)

**Scan Date:** 2026-04-19T17:30:00Z
**Snyk CLI Version:** 1.1303.0
**Result:** SKIPPED — Snyk does not support CMake/C++ projects for dependency vulnerability scanning.

The project has no `package.json`, `pom.xml`, `requirements.txt`, or other supported package manifest. Snyk reported:
> "Could not detect supported target files"

**Recommendation:** Use alternative tools for C++ dependency vulnerability scanning:
- OSS Review Toolkit (ORT) for SBOM generation
- Conan/vcpkg audit commands if package managers are adopted
- Manual CVE monitoring for: onnxruntime, libopus, Qt6

### Code (SAST) Scan
Snyk Code exited with code 2 (not available for C++ projects without Snyk for C++).

### IaC Scan
Not applicable — no Dockerfile, docker-compose, or Kubernetes config present.

