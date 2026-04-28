# pairion-client — Codebase Audit

**Generated:** 2026-04-28T00:52:30Z
**Branch:** main
**Commit:** d72ae99aeb5a413bcb94c08fd28fa69b50e44125 feat: memory browser UI — MemoryClient, MemoryBrowserModel, slide-in panel (PC-004)

---

## 1. Project Identity

```
Project Name:         pairion-client
Repository URL:       https://github.com/Pairion-ai/Pairion-Client.git
Primary Language:     C++20 / QML (Qt Quick)
Framework:            Qt 6.11.0
Language Version:     C++20, Apple Clang 17.0.0
Build Tool:           CMake 4.2.3 + Ninja 1.13.2
Package Manager:      vcpkg (via CMakePresets.json) / Homebrew (system Qt/Opus/ONNX)
Current Branch:       main
Latest Commit Hash:   d72ae99aeb5a413bcb94c08fd28fa69b50e44125
Latest Commit Msg:    feat: memory browser UI — MemoryClient, MemoryBrowserModel, slide-in panel (PC-004)
Audit Timestamp:      2026-04-28T00:52:30Z
```

---

## 2. Directory Structure

```
pairion-client/
├── CMakeLists.txt               — build system (all targets + tests)
├── CMakePresets.json            — 10 presets: macos-arm64-debug/release, linux-x64-debug/release, coverage, etc.
├── resources.qrc                — Qt resource bundle (all QML + assets)
├── Architecture.md              — architecture reference
├── Client-Architecture.md       — client-specific architecture
├── Pairion-Layer-Architecture.md — layer system architecture
├── Pairion-Scene-Architecture.md — scene/HUD architecture
├── cmake/
│   ├── check_coverage.cmake     — LCOV threshold enforcement (100% required)
│   └── filter_coverage.cmake    — LCOV filter script (excludes moc, build, test)
├── platform/macos/Info.plist    — macOS bundle config (mic permission, app metadata)
├── qml/
│   ├── Main.qml                 — root ApplicationWindow (FullScreen, keyboard shortcuts)
│   ├── Backgrounds/             — background layers (Globe, Space, Dark, OSM, VFR) — 5 files
│   ├── Debug/DebugPanel.qml     — developer debug panel (toggled by F12)
│   ├── HUD/                     — HUD components (10 files: PairionHUD, TopBar, RingSystem,
│   │                                LayerManager, DashboardPanels, TranscriptStrip, FpsCounter,
│   │                                HemisphereMap, PipelineHealthIndicator, MemoryBrowser)
│   ├── Overlays/                — overlay layers (AdsbOverlay, WeatherCurrentOverlay, WeatherRadarOverlay) — 3 files
│   └── PairionScene/            — design SDK (PairionStyle singleton, BackgroundBase, OverlayBase)
├── resources/
│   ├── adsb/icons/              — 7 PNG aircraft type icons
│   ├── adsb/vfr_sectional.png   — VFR sectional chart background
│   ├── maps/                    — world_map_day.jpg + world_map_night.jpg
│   └── model_manifest.json      — ONNX model SHA-256 manifest
├── src/
│   ├── audio/                   — capture, playback, Opus encoder/decoder, ring buffer
│   ├── core/                    — constants, device identity, ONNX session, model downloader
│   ├── memory/                  — MemoryClient (HTTP), MemoryBrowserModel (QML singleton)
│   ├── pipeline/                — AudioSessionOrchestrator, PipelineHealthMonitor
│   ├── protocol/                — BinaryCodec, EnvelopeCodec, messages.h
│   ├── settings/                — Settings (QSettings wrapper)
│   ├── state/                   — ConnectionState (central QML-exposed state)
│   ├── util/                    — Logger (batch HTTP log forwarder)
│   ├── vad/                     — SileroVad (ONNX-based VAD)
│   ├── wake/                    — OpenWakewordDetector (ONNX-based wake word)
│   ├── ws/                      — PairionWebSocketClient (WS state machine)
│   └── main.cpp                 — application entry point
├── test/                        — 19 test executables (QtTest framework)
│   ├── mock_server.h/.cpp       — mock WebSocket server for integration tests
│   ├── mock_onnx_session.h      — mock ONNX inference session
│   └── tst_*.cpp                — one file per module (21 test files)
└── scripts/install/             — installation helper scripts
```

Single-module C++20 application. Source is split into a `pairion_core` STATIC library (all `src/**` except `main.cpp`) and a `pairion` executable. All 19 test executables link against `pairion_core`. QML files are embedded via Qt resources (`.qrc`).

---

## 3. Build & Dependency Manifest

**Build file:** `CMakeLists.txt` (CMake 3.25+, project version 0.3.0)
**Presets file:** `CMakePresets.json` — 10 configure presets: macos-arm64-debug/release, macos-x86_64-debug/release, linux-x86_64-debug/release, linux-arm64-debug/release, windows-x86_64-debug/release, plus matching build and test presets.

### Dependencies

| Dependency | Version | Purpose |
|---|---|---|
| Qt6::Core | 6.11.0 | QObject, signals/slots, QString, QTimer, QThread |
| Qt6::Quick | 6.11.0 | QML engine, QQmlApplicationEngine, qmlRegisterSingletonInstance |
| Qt6::Network | 6.11.0 | QNetworkAccessManager, QNetworkReply (WS + HTTP) |
| Qt6::WebSockets | 6.11.0 | QWebSocket (bidirectional real-time comms to server) |
| Qt6::Multimedia | 6.11.0 | QAudioSource, QAudioSink, QMediaDevices |
| Qt6::Test | 6.11.0 | QtTest framework (QTEST_GUILESS_MAIN, QSignalSpy, QTest::qWait) |
| Opus | 1.6.1 | Audio codec (libopus via PkgConfig) — encode PCM→Opus, decode Opus→PCM |
| onnxruntime | (system) | ONNX model inference — wake word detection + Silero VAD |

### Build Commands

```
Configure:  cmake --preset macos-arm64-debug
Build:      cmake --build --preset macos-arm64-debug
Test:       ctest --preset macos-arm64-debug
Coverage:   cmake --build --preset macos-arm64-debug --target coverage
  (requires -DPAIRION_COVERAGE=ON; uses LCOV + genhtml; threshold=100%)
Run:        ./build/macos-arm64-debug/pairion.app/Contents/MacOS/pairion
```

### Build Options

| Option | Default | Effect |
|---|---|---|
| PAIRION_COVERAGE | OFF | Enables --coverage flags + LCOV coverage target |
| PAIRION_WAKE_DIAGNOSTICS | OFF | Enables Info-level wake score logging |
| PAIRION_CLIENT_NATIVE_TESTS | OFF | Enables tests requiring real ONNX model files |
| PAIRION_COVERAGE_THRESHOLD | 100 | Minimum line coverage % enforced at build time |

---

## 4. Configuration & Infrastructure Summary

### Configuration Files

| File | Purpose | Key Values |
|---|---|---|
| `CMakePresets.json` | Build presets for all platforms | 10 presets; generator=Ninja; BUILD_SHARED_LIBS=OFF |
| `platform/macos/Info.plist` | macOS bundle metadata | NSMicrophoneUsageDescription; CFBundleIdentifier=ai.pairion.client; CFBundleVersion=0.3.0 |
| `src/core/constants.h` | Compile-time application constants | kDefaultServerUrl="ws://localhost:18789/ws/v1"; kDefaultRestBaseUrl="http://localhost:18789/v1"; heartbeat=15s; log flush=30s; reconnect backoffs 1–30s |
| `resources/model_manifest.json` | ONNX model SHA-256 manifest | Lists melspectrogram.onnx, embedding_model.onnx, hey_jarvis_v0.1.onnx, silero_vad.onnx with download URLs + SHA-256 hashes |
| `resources.qrc` | Qt resource bundle | Embeds all QML files, maps, aircraft icons, model manifest |
| `cmake/check_coverage.cmake` | LCOV coverage gate | Reads filtered coverage.info; fails build if lines < PAIRION_COVERAGE_THRESHOLD (100) |

### Connection Map

```
WebSocket Server: ws://localhost:18789/ws/v1 (dev; kDefaultServerUrl)
REST API Server:  http://localhost:18789/v1  (dev; kDefaultRestBaseUrl)
  → POST /v1/logs            — log batch forwarding (Logger)
  → GET  /v1/memory/episodes — memory browser episodes (MemoryClient)
  → GET  /v1/memory/episodes/{id}/turns — episode turns (MemoryClient)
  → GET  /v1/memory/preferences — user preferences (MemoryClient)
Model CDN:        GitHub releases (ModelDownloader downloads ONNX files on first run)
Database:         None
Cache:            None
Message Broker:   None (WebSocket is the real-time channel)
External APIs:    None beyond above
Cloud Services:   None
```

### Environment Variables

None — all configuration is compile-time constants in `src/core/constants.h`. Production deployment will require runtime-configurable server URLs (see Section 21).

### CI/CD

No CI/CD pipeline detected. No `.github/workflows/`, `Jenkinsfile`, or `.gitlab-ci.yml` present.

---

## 5. Startup & Runtime Behavior

**Entry point:** `src/main.cpp` — `int main(int argc, char *argv[])`

### Startup Sequence

1. `QGuiApplication` created; app name="Pairion", org="Pairion", version from `version.h` (CMake configure_file).
2. `QNetworkAccessManager` created (shared across Logger, ModelDownloader, MemoryClient).
3. `ConnectionState` (central QML singleton) and `Settings` (QSettings wrapper) created.
4. `Logger` constructed with NAM + REST base URL; log callback wired to `ConnectionState.appendLog`; `logger->install()` replaces Qt message handler.
5. `DeviceIdentity` loaded (UUID + bearer token from QSettings).
6. `PairionWebSocketClient` constructed; connects to `ws://localhost:18789/ws/v1`.
7. `PairionAudioCapture` + `PairionOpusEncoder` created; encoder moved to `EncoderThread`.
8. `MemoryClient` + `MemoryBrowserModel` created; model registered as QML singleton `"MemoryBrowserModel"`.
9. QML singletons registered: `ConnectionState`, `Settings`, `MemoryBrowserModel`.
10. `QQmlApplicationEngine` loads `qrc:/qml/Main.qml`.
11. `ModelDownloader` created; wired to `sessionOpened` signal — downloads missing ONNX models on first WS connection.
12. Microphone permission requested via `QMicrophonePermission` API.
13. `wsClient->connectToServer()` — WS connection starts.
14. When BOTH mic permission granted AND models ready → `initAudioPipeline()` called.
15. `initAudioPipeline()`: loads ONNX sessions, creates `OpenWakewordDetector` + `SileroVad` (moved to `InferenceThread`), creates `PairionAudioPlayback` + `AudioSessionOrchestrator`, starts capture. ONNX load retries up to 3× with 1s delay. Sets `pipelineHealth="ready"`.
16. `PipelineHealthMonitor` started — watches threads and WS for anomalies.

### Threading Model

| Thread | Objects |
|---|---|
| Main thread | QML, `PairionAudioCapture`, `ConnectionState`, `PairionWebSocketClient`, `AudioSessionOrchestrator` |
| EncoderThread | `PairionOpusEncoder` (CPU-bound Opus encoding) |
| InferenceThread | `OpenWakewordDetector`, `SileroVad` (ONNX inference per audio frame) |

### Health States (pipelineHealth property on ConnectionState)

`"connecting"` → `"models_loading"` → `"initializing"` → `"ready"` | `"mic_offline"` | `"models_failed"` | `"pipeline_error"`

### No Database, No Migrations, No Seed Data

This is a desktop client application. No persistence layer beyond QSettings.

---

## 6. Data Model / Domain Types

This is a desktop client application with no persistence layer (no ORM, no database). The "models" are QObject singletons and C++ structs.

---

### ConnectionState (`src/state/connection_state.h`)

Central QML-exposed singleton tracking all runtime state. Registered as `"ConnectionState"` singleton in the `Pairion` QML module.

**Q_PROPERTY bindings (all READ/NOTIFY, some WRITE via slots):**

| Property | Type | Default | Purpose |
|---|---|---|---|
| status | Status enum | Disconnected | WS connection state |
| sessionId | QString | "" | Server session ID |
| serverVersion | QString | "" | Server version string |
| reconnectAttempts | int | 0 | WS reconnect counter |
| recentLogs | QStringList | [] | Last 10 debug log entries |
| transcriptPartial | QString | "" | In-progress STT transcript |
| transcriptFinal | QString | "" | Final STT transcript |
| llmResponse | QString | "" | Accumulated LLM token stream |
| agentState | QString | "" | idle/listening/thinking/speaking |
| voiceState | QString | "" | idle/awaiting_wake/streaming/ending_speech |
| backgroundContext | QString | "earth" | HUD background context |
| mapFocusActive | bool | false | Server map focus active |
| mapFocusLat/Lon | double | 0.0 | Map focus coordinates |
| mapFocusLabel | QString | "" | Map focus display label |
| mapFocusZoom | QString | "" | continent/country/region/city |
| activeBackgroundId | QString | "globe" | Active background layer ID |
| activeOverlayIds | QStringList | [] | Active overlay layer IDs |
| sceneData | QVariantMap | {} | Accumulated SceneDataPush data keyed by modelId |
| backgroundParams | QVariantMap | {} | Params from last BackgroundChange |
| overlayParams | QVariantMap | {} | Per-overlay params keyed by overlayId |
| sceneTransition | QString | "crossfade" | Background transition style |
| pipelineHealth | QString | "connecting" | Pipeline health status string |

**Status enum:** `Disconnected=0`, `Connecting`, `Connected`, `Reconnecting`

---

### Settings (`src/settings/settings.h`)

QSettings-backed singleton. Registered as `"Settings"` in `Pairion` QML module.

| Property | Type | Purpose |
|---|---|---|
| wakeThreshold | double | Wake word detection confidence threshold |
| vadSilenceEndMs | int | Silence duration (ms) to end speech segment |
| vadThreshold | double | VAD probability threshold |
| audioInputDevice | QString | Audio input device name |
| audioSampleRate | int | PCM sample rate (typically 16000) |

---

### MemoryBrowserModel (`src/memory/memory_browser_model.h`)

QML singleton model backing the memory browser slide-in panel. Registered as `"MemoryBrowserModel"` in `Pairion` QML module.

| Property | Type | Purpose |
|---|---|---|
| episodes | QVariantList | List of episode maps: {id, title, startTime, turnCount} |
| selectedEpisodeTurns | QVariantList | Turns for selected episode: {role, content, timestamp} |
| preferences | QVariantList | User preferences: {key, value, updatedAt} |
| loading | bool | True while any HTTP request is in-flight |
| errorMessage | QString | Last fetch error or empty |
| selectedEpisodeId | QString | Currently selected episode ID or empty |
| hasMemory | bool | True when episodes list is non-empty |

**Slots:** `refresh()`, `selectEpisode(episodeId)`, `clearSelection()`

---

### Protocol Messages (`src/protocol/messages.h`)

C++ structs mapping to AsyncAPI message types. Not serialized to disk — created on decode.

**Outbound (Client → Server):** `DeviceIdentify`, `HeartbeatPing`, `WakeWordDetected`, `AudioStreamStartIn`, `SpeechEnded`, `AudioStreamEndIn`, `TextMessage` — combined as `OutboundMessage` std::variant.

**Inbound (Server → Client):** `SessionOpened`, `SessionClosed`, `HeartbeatPong`, `ErrorMessage`, `AgentStateChange`, `TranscriptPartial`, `TranscriptFinal`, `LlmTokenStream`, `ToolCallStarted`, `ToolCallCompleted`, `AudioStreamStartOut`, `AudioStreamEndOut`, `UnderBreathAck`, `MapFocus`, `MapClear`, `BackgroundChange`, `OverlayAdd`, `OverlayRemove`, `OverlayClear`, `SceneDataPush` — combined as `InboundMessage` std::variant.

---

## 7. Enum / Constant Inventory

### ConnectionState::Status (`src/state/connection_state.h`)
```
Values: Disconnected=0, Connecting, Connected, Reconnecting
Used in: ConnectionState Q_PROPERTY, PairionWebSocketClient, DebugPanel.qml
Serialization: Q_ENUM — exposed as int to QML
```

### AudioSessionOrchestrator::State (`src/pipeline/audio_session_orchestrator.h`)
```
Values: Idle, AwaitingWake, Streaming, EndingSpeech
Used in: AudioSessionOrchestrator state machine (internal)
Serialization: C++ enum class — not exposed to QML
```

### Pipeline Health Strings (`src/state/connection_state.h`, `src/main.cpp`)
String constants used as pipelineHealth values (not a C++ enum — stringly typed):
`"connecting"`, `"models_loading"`, `"initializing"`, `"ready"`, `"mic_offline"`,
`"wake_failed"`, `"server_disconnected"`, `"models_failed"`, `"pipeline_error"`

### Application Constants (`src/core/constants.h`)
| Constant | Value | Purpose |
|---|---|---|
| kDefaultServerUrl | "ws://localhost:18789/ws/v1" | Default WS server (dev only) |
| kDefaultRestBaseUrl | "http://localhost:18789/v1" | Default REST server (dev only) |
| kHeartbeatIntervalMs | 15000 | Heartbeat ping interval |
| kHeartbeatDeadlineMs | 10000 | Pong deadline before reconnect |
| kLogFlushIntervalMs | 30000 | Log batch flush interval |
| kReconnectBackoffMs[] | {1000,2000,4000,8000,15000,30000} | Reconnect backoff steps |
| kReconnectBackoffSteps | 6 | Number of backoff steps |
| kPcmFrameBytes | 640 | PCM frame size (20ms @16kHz mono 16-bit) |
| kPreRollBytes | 6400 | Pre-roll buffer (~200ms) |
| kStreamingTimeoutMs | 30000 | Safety cap for runaway streams |
| kWakeSuppressionMs | 500 | False-wake suppression window |

### Protocol Message Type Constants (`src/protocol/messages.h`)
Each protocol struct carries a `static constexpr const char *kType` tag used by EnvelopeCodec for JSON `"type"` field dispatch.

---

## 8. Data Access / Repository Layer

No separate repository or DAO layer exists. This is a desktop client application with no database. All persistent storage is via:

- **QSettings** — user preferences (Settings class), device identity (DeviceIdentity class)
- **Local filesystem** — ONNX model cache at `QStandardPaths::AppDataLocation + "/models/"`
- **HTTP GET** — memory data fetched via MemoryClient on demand (see Section 9)

Data access patterns are inline in service/component classes. See Section 9.

---

## 9. Service / Business Logic Layer

---

### Logger (`src/util/logger.h/.cpp`)

**Dependencies:** `QNetworkAccessManager` (injected), `ConnectionState` (via callback)

**Public Methods:**
- `Logger(nam, baseUrl, parent)` — constructs logger; does not install handler yet
- `~Logger()` — uninstalls message handler if installed
- `install()` — installs Qt message handler (`qInstallMessageHandler`), starts 30s flush timer
- `setSessionId(sessionId)` — attaches session ID to subsequent log records
- `setLogCallback(cb)` — registers callback for feeding records to debug panel
- `flush()` — immediately POSTs all buffered records to `POST /v1/logs`
- `pendingRecords()` — returns current buffer as QJsonArray (for testing)
- `pendingCount()` — returns count of buffered records

**Logging Category:** `Q_LOGGING_CATEGORY(lcLogger, "pairion.util.logger")`

---

### ModelDownloader (`src/core/model_downloader.h/.cpp`)

**Dependencies:** `QNetworkAccessManager` (injected)

**Public Methods:**
- `ModelDownloader(nam, parent)` — loads manifest from `:/resources/model_manifest.json`
- `checkAndDownload()` — verifies SHA-256 of each model; downloads any missing/corrupt
- `static modelCacheDir()` — returns `QStandardPaths::AppDataLocation + "/models"`
- `static modelPath(name)` — returns full path to a named model file

**Signals:** `downloadProgress(int)`, `allModelsReady()`, `downloadError(QString)`

---

### MemoryClient (`src/memory/memory_client.h/.cpp`)

**Dependencies:** `QNetworkAccessManager` (injected)

**Public Methods:**
- `MemoryClient(nam, baseUrl, parent)`
- `fetchEpisodes()` — GET `/v1/memory/episodes?userId=default-user` → emits `episodesReceived`
- `fetchTurns(episodeId)` — GET `/v1/memory/episodes/{id}/turns?userId=default-user` → emits `turnsReceived`
- `fetchPreferences()` — GET `/v1/memory/preferences?userId=default-user` → emits `preferencesReceived`

**Private:** `get(url, operation, onSuccess)` — shared async GET; handles array, object-with-items, network error, JSON parse error

**Logging Category:** `Q_LOGGING_CATEGORY(lcMemoryClient, "pairion.memory.client")`

---

### MemoryBrowserModel (`src/memory/memory_browser_model.h/.cpp`)

**Dependencies:** `MemoryClient` (injected)

**Public Slots:**
- `refresh()` — clears selection, sets pendingRequests=2, calls fetchEpisodes + fetchPreferences
- `selectEpisode(episodeId)` — sets selectedEpisodeId, calls fetchTurns, increments pending
- `clearSelection()` — clears selectedEpisodeId and selectedEpisodeTurns

**Private:** `setLoading(bool)`, `setErrorMessage(QString)`, `decrementPending()`, `static formatTimestamp(iso)` (ISO 8601 → locale display; passthrough on parse failure)

**Logging Category:** `Q_LOGGING_CATEGORY(lcMemoryModel, "pairion.memory.model")`

---

### PairionWebSocketClient (`src/ws/pairion_websocket_client.h/.cpp`)

**Dependencies:** `ConnectionState` (injected), `QWebSocket`

**Public Methods:**
- `PairionWebSocketClient(serverUrl, deviceId, bearerToken, connState, parent)`
- `connectToServer()` — initiates WS connection; begins heartbeat on connect
- `disconnectFromServer()` — closes WS cleanly
- `sendMessage(OutboundMessage)` — serializes and sends a text-frame WS message
- `setHeartbeatIntervalMs(int)` — overrides default 15s heartbeat (used in tests)
- `sendBinaryFrame(QByteArray)` — sends raw binary WS frame (Opus audio)

**Reconnect:** exponential backoff using kReconnectBackoffMs[]; up to kReconnectBackoffSteps retries

**Signals:** `sessionOpened(sessionId, serverVersion)`, `sessionClosed(reason)`, `serverError(code, message)`, `disconnected()`, `reconnecting()`, `reconnected(sessionId, serverVersion)`

---

### AudioSessionOrchestrator (`src/pipeline/audio_session_orchestrator.h/.cpp`)

**Dependencies:** `PairionAudioCapture`, `PairionOpusEncoder`, `OpenWakewordDetector`, `SileroVad`, `PairionWebSocketClient`, `ConnectionState`, `PairionAudioPlayback`

**State machine:** `Idle → AwaitingWake → Streaming → EndingSpeech → Idle`

**Public Methods:**
- `startListening()` — transitions to `AwaitingWake`
- `shutdown()` — stops capture, resets state to `Idle`
- `state()` — returns current `State` enum value

**Signals:** `wakeFired(streamId)`, `streamEnded(streamId)`, `pipelineError(message)`

---

### PipelineHealthMonitor (`src/pipeline/pipeline_health_monitor.h/.cpp`)

**Dependencies:** `PairionWebSocketClient`, `PairionAudioCapture`, `QThread` (encoderThread + inferenceThread), `AudioSessionOrchestrator`, `ConnectionState`

**Public Slots:**
- `start()` — begins health checks (5s initial delay, then every 30s)
- `performHealthCheck()` — checks WS connected, threads running, audio frames arriving; updates pipelineHealth via ConnState

**Signals:** `healthChanged(health)`

---

### DeviceIdentity (`src/core/device_identity.h/.cpp`)

Loads or generates a stable UUID device ID and bearer token from QSettings.

**Methods:** `loadOrCreate()`, `deviceId()`, `bearerToken()`

---

## 10. QML Layer — Component Map

This is a QML client, not a REST API server. There are no HTTP controllers. The equivalent is the QML component hierarchy wired to `ConnectionState` bindings.

---

### Main.qml — Root Window
- `ApplicationWindow` (FullScreen, `color: "#0a0e1a"`)
- Renders `PairionHUD` (visible when `hudActive=true`) or `DebugPanel` overlay
- **Keyboard shortcuts:** F12 (toggle debug), F11 (toggle FPS), M (toggle memory browser), Escape (quit), B (cycle backgrounds), A (toggle adsb overlay), W (toggle weather_radar overlay), 0–5 (focusPin)

### PairionHUD.qml — Full-Screen HUD
- `LayerManager` — background + overlay layers, driven by `ConnectionState.activeBackgroundId/activeOverlayIds`
- `RingSystem` — animated ring overlay, visible only when `agentState == "speaking"`
- `TopBar` — connection status, clock, session info
- `DashboardPanels` — 4 info panels (News, Inbox, Tasks, Homestead) — placeholder data pending M3+ SceneDataPush
- `TranscriptStrip` — live STT transcript display
- `FpsCounter` — optional FPS overlay (F11 toggle)
- `PipelineHealthIndicator` — bottom-left health LED
- `MemoryBrowser` — right-side slide-in panel (M key toggle, `memoryBrowserVisible` property)

### LayerManager.qml
Dynamically loads QML backgrounds from `qrc:/qml/Backgrounds/` and overlays from `qrc:/qml/Overlays/` by convention (ID → filename). Supports crossfade and instant transitions.

### Background Components (qml/Backgrounds/)
| File | ID | Description |
|---|---|---|
| GlobeBackground.qml | "globe" | Animated world map globe with news pins, day/night, MapFocus |
| SpaceBackground.qml | "space" | Starfield parallax background |
| DarkBackground.qml | "dark" | Solid dark panel |
| OsmBackground.qml | "osm" | OpenStreetMap dark-styled tile map |
| VfrBackground.qml | "vfr" | VFR sectional chart image |

### Overlay Components (qml/Overlays/)
| File | ID | Description |
|---|---|---|
| AdsbOverlay.qml | "adsb" | ADS-B aircraft radar — PNG icons, ICAO24 dedup, type display |
| WeatherCurrentOverlay.qml | "weather_current" | Current weather conditions overlay |
| WeatherRadarOverlay.qml | "weather_radar" | RainViewer animated weather radar tiles |

### MemoryBrowser.qml
Right-side slide-in panel (280ms Easing.OutCubic). Width = max(400px, 30% of parent). Tabs: Episodes (default) and Preferences. Episode list → turn detail view (chat-style bubbles, user/assistant differentiated). Auto-refreshes via `MemoryBrowserModel.refresh()` on open. Driven by `MemoryBrowserModel` singleton.

---

## 11. Security Configuration

This is a desktop client, not a web server. Security is handled at the transport and identity layer.

```
Authentication:     Bearer token — UUID token generated on first launch, stored in
                    QSettings (platform-native secure storage on macOS = Keychain-backed).
                    Sent as bearerToken field in DeviceIdentify WS message.
Token issuer:       Local — DeviceIdentity generates and persists the token
Device ID:          UUID v4 generated on first launch, stored in QSettings
Password hashing:   N/A — no passwords. Bearer token is a random UUID.

Transport (WS):     ws:// (unencrypted — DEVELOPMENT ONLY)
                    Production requires wss:// — see Section 21 (Non-Blocking Observation)
Transport (REST):   http:// (unencrypted — DEVELOPMENT ONLY)

CORS:               N/A — desktop client, no browser context
CSRF:               N/A — no web session
Rate limiting:      None — single-user desktop client
```

### ONNX Model Integrity

All ONNX models are SHA-256 verified before use. `ModelDownloader.verifyFile()` computes the hash and compares against `resources/model_manifest.json`. Mismatched models are re-downloaded.

### Microphone Permission

macOS mic permission requested via `QMicrophonePermission` API at startup. `platform/macos/Info.plist` declares `NSMicrophoneUsageDescription`. No audio is captured until permission is granted.

---

## 12. Custom Security Components

### DeviceIdentity (`src/core/device_identity.h/.cpp`)

```
Type:               QObject credential manager
Purpose:            Loads or generates device UUID + bearer token from QSettings on first launch
Credential storage: QSettings (macOS: NSUserDefaults / plist; future: Keychain integration)
Credentials used:   DeviceIdentify WS message fields (deviceId, bearerToken)
```

No authentication middleware, no JWT validation, no OAuth2 flows. The client only presents credentials; the server validates them.

---

## 13. Exception / Error Handling

No exception-based error handling system. Error handling patterns:

**Audio Pipeline:** `PairionAudioCapture` emits `captureError(QString)`. `AudioSessionOrchestrator` emits `pipelineError(QString)`. Both wired to `ConnectionState.appendLog()` in `main.cpp`.

**ONNX Loading:** try/catch around `OrtInferenceSession` construction in `initAudioPipeline()`. Retries up to 3× with 1s delay. On exhaustion: `connState->setPipelineHealth("models_failed")`.

**WebSocket:** `PairionWebSocketClient` handles `QWebSocket::errorOccurred` and `disconnected` signals; reconnects with exponential backoff. Emits `serverError(code, message)` for server-sent error frames.

**HTTP (Logger):** `QNetworkReply::error` checked in finished handler; logged via `qCWarning`. No UI notification for log flush failures.

**HTTP (MemoryClient):** `QNetworkReply::error` and JSON parse failures emit `fetchError(operation, message)`. `MemoryBrowserModel` surfaces this as `errorMessage` property displayed in the browser panel.

**Thread monitoring:** `QThread::finished` signals wired in `initAudioPipeline()` — unexpected thread exit logs CRITICAL and surfaces to debug panel.

**No global exception filter.** Qt does not use exception-driven error flows by convention.

---

## 14. Mappers / Data Transformation

No dedicated mapper layer. Transformations occur at codec boundaries:

### EnvelopeCodec (`src/protocol/envelope_codec.h/.cpp`)

Stateless, all-static. Converts between `OutboundMessage`/`InboundMessage` variants and JSON text frames.

| Method | Transforms |
|---|---|
| `serialize(OutboundMessage)` → QJsonObject | C++ struct → JSON object |
| `serializeToString(OutboundMessage)` → QString | C++ struct → compact JSON string |
| `deserialize(QJsonObject)` → optional\<InboundMessage\> | JSON object → typed C++ struct |
| `deserializeFromString(QString)` → optional\<InboundMessage\> | Raw JSON string → typed C++ struct |

Dispatch: reads `"type"` field, constructs the matching struct type.

### BinaryCodec (`src/protocol/binary_codec.h/.cpp`)

Stateless, all-static. Handles binary WS audio frame layout.

| Method | Transforms |
|---|---|
| `encodeBinaryFrame(streamId, opusPayload)` | UUID + Opus bytes → 4-byte prefix + payload |
| `decodeBinaryFrame(frame)` | Raw binary frame → DecodedBinaryFrame {prefix, payload} |
| `streamIdToPrefix(streamId)` | UUID string → 4 bytes (RFC 4122 time_low, big-endian) |

### MemoryBrowserModel — Timestamp Formatting

`static formatTimestamp(iso: QString) → QString`

Converts ISO 8601 timestamps from memory API responses to locale display strings (e.g. "Jan 15, 10:30 AM"). Applied to all `startTime`, `timestamp`, and `updatedAt` fields before storage in QVariantList. Returns original string unchanged on parse failure.

---

## 15. Utility Modules & Shared Components

### RingBuffer<T, Capacity> (`src/audio/ring_buffer.h`)

Header-only, lock-free SPSC ring buffer. Capacity must be a power of two.

```
push(item: T) → bool   — enqueue; returns false if full
pop(item: T&) → bool   — dequeue; returns false if empty
size() → size_t         — current element count
```

Used by `PairionAudioCapture` for pre-roll buffering of PCM frames.

### PairionAudioCapture (`src/audio/pairion_audio_capture.h/.cpp`)

QAudioSource-backed PCM capture. Runs on main thread (macOS requirement).

```
configure(deviceId, sampleRate)   — set input device + rate (live reconfigure)
start()                           — begin capture
stop()                            — stop capture
Signals: audioFrameAvailable(QByteArray), captureError(QString)
```

### PairionOpusEncoder (`src/audio/pairion_opus_encoder.h/.cpp`)

CPU-bound Opus encoder, runs on EncoderThread.

```
encodePcmFrame(pcm20ms: QByteArray)  — encode 20ms PCM frame
isValid() → bool                      — returns true if Opus encoder initialized
Signals: opusFrameEncoded(QByteArray), encoderError(QString)
Constants: kSampleRate=16000, kBitrate=28000, kMaxPacketBytes=4000
```

### PairionOpusDecoder (`src/audio/pairion_opus_decoder.h/.cpp`)

Decodes inbound server audio (Opus → PCM) for TTS playback.

### PairionAudioPlayback (`src/audio/pairion_audio_playback.h/.cpp`)

QAudioSink-backed PCM playback for TTS audio from the server.

### OpenWakewordDetector (`src/wake/open_wakeword_detector.h/.cpp`)

ONNX-based wake word detection. Runs on InferenceThread. Uses three ONNX models (mel spectrogram, embedding, classifier). Pre-roll buffer accumulates recent audio for context.

```
warmup()                          — initializes ONNX sessions (queued connection)
processPcmFrame(pcm20ms)          — runs mel→embed→classify pipeline per frame
Signals: wakeWordDetected(score, preRollBuffer)
```

### SileroVad (`src/vad/silero_vad.h/.cpp`)

ONNX-based Voice Activity Detection. Runs on InferenceThread.

```
processPcmFrame(pcm20ms)   — runs Silero VAD ONNX model
reset()                     — resets VAD state
Signals: speechStarted(), speechEnded()
```

### Logger (`src/util/logger.h/.cpp`)

See Section 9. Also provides `Q_LOGGING_CATEGORY` infrastructure used as the standard logging mechanism across all modules.

**Logging categories in use:**
- `pairion.ws.client` — WebSocket client
- `pairion.audio.capture` — audio capture
- `pairion.audio.encoder` — Opus encoder
- `pairion.audio.decoder` — Opus decoder
- `pairion.audio.playback` — TTS playback
- `pairion.wake.detector` — wake word
- `pairion.vad.silero` — VAD
- `pairion.pipeline.orchestrator` — audio session
- `pairion.pipeline.health` — health monitor
- `pairion.core.downloader` — model download
- `pairion.util.logger` — logger itself
- `pairion.memory.client` — memory HTTP client
- `pairion.memory.model` — memory browser model

### PairionStyle.qml (`qml/PairionScene/PairionStyle.qml`)

QML singleton design token registry. Imported as `import PairionScene 1.0`.

```
Colors: cyan=#00b4ff, dim=#e0e8f0, amber=#ff8800, error=#ff3c3c,
        panelBg=#0d1220, darkBg=#070c18, borderColor=#00b4ff,
        gaColor=#00b4ff, airlineColor=#ff8800
Fonts:  fontFamily="Courier New", fontSizeXs=9, fontSizeSm=10, fontSizeMd=11, fontSizeLg=14
Spacing: radiusSm=4, borderWidth=1
```

---

## 16. Database Schema (Live)

No database. This is a desktop client application. All persistent storage is:

- **QSettings** — `~Library/Preferences/ai.pairion.pairion.plist` (macOS) — device identity, user settings
- **Model cache** — `~/Library/Application Support/Pairion/Pairion/models/*.onnx` — ONNX model files
- **Memory data** — fetched on demand from server REST API; not cached locally

No schema to document.

---

## 17. Message Broker Configuration

No traditional message broker (RabbitMQ, Kafka, etc.). The WebSocket connection to `ws://localhost:18789/ws/v1` serves as the bidirectional real-time channel.

**WebSocket protocol:** JSON text frames (typed by `"type"` field) + binary frames (Opus audio with 4-byte stream ID prefix). See Section 6 for message types and Section 14 for codec details.

---

## 18. Cache Layer

No caching layer. ADS-B aircraft data is received live via `SceneDataPush` WS messages and held in `ConnectionState.sceneData` (in-memory, volatile). Memory browser data is fetched on-demand via HTTP and not cached.

---

## 19. Environment Variable Inventory

No environment variables. All configuration is compile-time constants (`src/core/constants.h`) or QSettings-persisted values (`src/settings/settings.h`).

For production deployment, the following constants in `src/core/constants.h` will need to become runtime-configurable (env var or config file):

| Constant | Current Value | Production Requirement |
|---|---|---|
| `kDefaultServerUrl` | `ws://localhost:18789/ws/v1` | Must point to prod WS server |
| `kDefaultRestBaseUrl` | `http://localhost:18789/v1` | Must be `https://` prod URL |

---

## 20. Service Dependency Map

```
pairion-client → pairion-server
  WebSocket:  ws://localhost:18789/ws/v1  (authenticated via DeviceIdentify)
  REST:       http://localhost:18789/v1
    POST /v1/logs                           — batch log forwarding (Logger)
    GET  /v1/memory/episodes                — memory episodes (MemoryClient)
    GET  /v1/memory/episodes/{id}/turns     — episode turns (MemoryClient)
    GET  /v1/memory/preferences             — user preferences (MemoryClient)
  GitHub Releases: ONNX model downloads (ModelDownloader, first-run only)

Downstream consumers: None — this is the client. No services call into it.
```

---

## 21. Known Technical Debt & Issues

### TODO/Stub Scan Results

| Issue | Location | Severity | Notes |
|---|---|---|---|
| "TODO" string in comment (HudLayout doc header) | `qml/HUD/HudLayout.qml:12` | LOW | Panel layout comment mentions "TODO" as a panel label in a doc comment — not a code stub. HudLayout is NOT registered in resources.qrc and has zero QML import references — it is an orphaned/inactive file left over from an earlier HUD layout experiment. |
| Placeholder data in DashboardPanels | `qml/HUD/DashboardPanels.qml:5` | ACKNOWLEDGED | Documented intentional placeholder — all 4 panels (News, Inbox, Tasks, Homestead) will become data-driven via SceneDataPush in M3+ milestones. Comment block added in PC-STUB-001 records intent. |
| Placeholder comment in WeatherCurrentOverlay | `qml/Overlays/WeatherCurrentOverlay.qml:76` | LOW | Section heading comment uses "placeholder" — refers to a loading/empty state UI section, not an unimplemented feature stub. Implementation is active. |

### Non-Blocking Observations

1. **No CI/CD** — No GitHub Actions, Jenkinsfile, or any automated quality gate. Build, test, and coverage run must be triggered manually.
2. **Hardcoded server URLs** — `kDefaultServerUrl` and `kDefaultRestBaseUrl` are compile-time constants pointing to `localhost:18789`. No runtime-configurable server URL for production deployment.
3. **ws:// (unencrypted)** — WebSocket and REST connections use plaintext protocols. Production requires `wss://` and `https://`.
4. **userId hardcoded** — `MemoryClient` uses `"default-user"` as the userId query parameter. Will need to come from `DeviceIdentity` or a user profile when multi-user support is added.
5. **HudLayout.qml orphaned** — `qml/HUD/HudLayout.qml` and `qml/HUD/DashboardPanel.qml` are not registered in `resources.qrc` and have no import references. These are inactive files from a prior layout experiment and can be deleted.
6. **No openapi.yaml** — Project has no `openapi.yaml`. REST endpoints are derived from the task specification. An OpenAPI spec should be generated.
7. **Error handling incomplete in hot paths** — `Code Quality` scorecard notes 1/2 on error handling: some signal error paths (particularly in audio hot paths) do not surface errors through the full notification chain to the UI.

---

## 22. Security Vulnerability Scan (Snyk)

```
Scan Date:         2026-04-28T00:52:30Z
Snyk CLI Version:  1.1303.0
```

### Dependency Vulnerabilities (Open Source)

```
Critical: 0
High:     0
Medium:   0
Low:      0

RESULT: PASS — No known vulnerabilities in dependencies.
```

### Code Vulnerabilities (SAST — Snyk Code)

```
Errors:   0
Warnings: 0
Notes:    0

RESULT: PASS — No code vulnerabilities detected.
```

### IaC Findings

No Dockerfile, docker-compose.yml, or Kubernetes config present. IaC scan not applicable.
