# Changelog

All notable changes to Pairion Client will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [0.2.1] - 2026-04-17

### Added

- Opus codec integration via `audiopus` crate (encode 16kHz mono, decode configurable rate)
- PairionOpusEncoder: 20ms frames, VBR, for AudioChunkIn binary WS frames
- PairionOpusDecoder: accepts AudioChunkOut binary WS frames, outputs PCM
- Encode/decode round-trip unit tests verifying lossy fidelity
- Outbound WebSocket message channel (mpsc): enables Tauri commands and audio pipeline to send messages to Server
- OutboundSender/OutboundReceiver with backpressure support (bounded channel, 256 capacity)
- WS client consumes outbound channel alongside heartbeats and inbound messages
- AppState now holds OutboundSender for shared access across commands
- ONNX Runtime (`ort`) dependency added for future wake-word and VAD model loading
- Demo pending stub at docs/demos/DEMO_PENDING.md
- README updated with `brew install opus` and `brew install pkg-config` prerequisites

## [0.2.0] - 2026-04-17

### Added

- M1 First Voice: Audio capture/playback pipeline with cpal + ring buffers
- AudioCaptureManager with non-blocking cpal input callback and SPSC ring buffer
- AudioPlaybackManager with jitter buffer and cpal output callback
- AudioPipeline orchestrator (capture + playback + pre-wake buffer)
- PreWakeBuffer: 200ms circular buffer for retaining initial syllable before wake
- Wake-word detection module with trait-based WakeWordDetector abstraction
- EnergyWakeDetector for development testing (simulates wake-word with audio energy)
- False-wake suppression within 500ms window
- VAD module with trait-based VoiceActivityDetector abstraction
- EnergyVad for development (800ms silence threshold triggers end-of-speech)
- Extended WebSocket messages: WakeWordDetected, AudioStreamStart/End, SpeechEnded, TextMessage, SessionOpened/Closed, TranscriptPartial/Final, LlmTokenStream, AgentStateChange, ToolCallStarted/Completed
- WebSocket message dispatch for all M1 inbound message types
- Binary WebSocket frame handling for AudioChunkOut (TTS audio)
- Session state management with AgentState enum and watch channel
- Voice-state indicator in HUD: Idle/Listening/Thinking/Speaking badges
- Live transcript strip showing partial and final STT text
- Implemented Tauri commands: start_listening_session, stop_current_session, send_text_message, get_session_state
- 119 Rust tests + 55 React tests

## [0.1.0] - 2026-04-17

### Added

- M0 Walking Skeleton: Tauri 2 application with React + TypeScript + Tailwind frontend
- WebSocket client with exponential-backoff reconnection to Pairion Server
- Connection status window showing Connecting/Connected/Disconnected/Reconnecting states
- Heartbeat ping/pong loop (15s interval, 30s timeout)
- Bearer token management via keychain abstraction with file fallback
- Secret scrubber utility for log output sanitization
- Structured logging via `tracing` crate with log forwarding stub
- Zustand stores: connectionStore, settingsStore, sessionStore (scaffold), hudStore (scaffold), cardsStore (scaffold)
- Screen capture indicator component (invariant scaffold — never activated in M0)
- Logger utility for React frontend (forwards to Rust via Tauri commands)
- Tauri commands: get_connection_state, get_device_id, is_server_reachable, forward_log
- Scaffolded commands for later milestones (identify_device, start_listening_session, etc.)
- Scaffolded Rust modules: audio, screen, computer_use, config, state
- Mock WebSocket server test harness
- Full test suite (Rust + React) with 100% coverage targets
- GitHub Actions CI pipeline
- Repository meta: README, CONTRIBUTING, CODE_OF_CONDUCT, LICENSE, CHANGELOG
- RustDoc on every pub item; JSDoc on every exported component/hook/utility
