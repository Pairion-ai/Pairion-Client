# Changelog

All notable changes to Pairion-Client will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [0.2.0] - 2026-04-18

### Added

- Audio capture at 16 kHz mono 16-bit via QAudioSource with worker-thread isolation
- Opus encoder (28 kbps VOIP, 20 ms frames) and decoder via libopus
- Lock-free SPSC ring buffer (header-only template) for audio pipeline
- ONNX inference session abstraction with real OnnxRuntime and mock implementations
- OpenWakeWord three-model ONNX pipeline (melspectrogram → embedding → hey_jarvis) with 500 ms false-wake suppression and ~200 ms pre-roll buffer
- Silero VAD ONNX model with recurrent state management, configurable silence threshold (800 ms default)
- Audio session orchestrator with Idle → AwaitingWake → Streaming → EndingSpeech state machine and 30-second safety cap
- Binary WebSocket frame codec with 4-byte UUID stream ID prefix
- Model downloader with SHA-256 verification and AppDataLocation caching
- QSettings-backed Settings singleton (wake threshold, VAD thresholds, audio device, sample rate)
- ConnectionState extended with transcriptPartial, transcriptFinal, llmResponse, agentState, voiceState
- WebSocket client extended with sendBinaryFrame and typed signals for TranscriptPartial/Final, AgentStateChange, LlmTokenStream
- Debug panel extended with transcript, LLM response, agent state, and voice pipeline state sections
- 8 new test suites: binary frame, settings, ring buffer, opus codec, wake detector, silero VAD, model downloader, audio session orchestrator

## [0.1.1] - 2026-04-18

### Fixed

- Restored 100% line coverage compliance (CONVENTIONS §3) by annotating 7 genuinely-unreachable lines with LCOV exclusion directives and technical justifications
- Added CMake coverage threshold enforcement (`PAIRION_COVERAGE_THRESHOLD=100`) that fails the build if line coverage drops below 100%
- Added `cmake/check_coverage.cmake` script for automated LCOV tracefile parsing and threshold checking

## [0.1.0] - 2026-04-18

### Added

- Project scaffolding: CMake build system with cross-platform presets (macOS, Windows, Linux)
- WebSocket client (`PairionWebSocketClient`) with DeviceIdentify, SessionOpened, HeartbeatPing/Pong
- Exponential backoff reconnection (1s → 2s → 4s → 8s → 15s → 30s cap)
- Protocol message types for all AsyncAPI messages with JSON envelope codec
- ConnectionState QML singleton with status, session ID, server version, reconnect attempts
- Debug panel QML UI with colored status dot, session info, and scrolling log display
- Centralized logging via `qInstallMessageHandler` with structured JSON and batched forwarding to `/v1/logs`
- Device identity persistence via QSettings (device ID + bearer token)
- Mock WebSocket server harness for deterministic integration testing
- Unit tests for message codec, log batching, and WebSocket state machine
- Integration tests for full connect → identify → heartbeat → disconnect → reconnect lifecycle
- Platform placeholders for macOS Info.plist with NSMicrophoneUsageDescription
- Module placeholders for audio, wake, vad, pipeline, hud, settings (M1+)
