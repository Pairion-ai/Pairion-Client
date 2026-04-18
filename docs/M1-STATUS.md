# M1 First Voice — Code-Complete Status

## Commit

**SHA:** (see final commit of PC-002c)  
**Branch:** main

## Code-Complete Checklist

### Verified by CI/tests (automated)

- [x] cargo fmt -- --check passes
- [x] cargo clippy --deny warnings passes (0 warnings)
- [x] cargo build --workspace succeeds
- [x] cargo test --workspace: 153 tests pass (148 unit + 5 integration)
- [x] pnpm lint passes (0 warnings)
- [x] pnpm typecheck passes
- [x] pnpm test: 55 tests pass (8 test files)
- [x] No console.log in src/; no println! in src-tauri/src/
- [x] RustDoc on every pub item
- [x] JSDoc on every exported component/hook/utility

### ONNX Model Integration

- [x] melspectrogram.onnx downloaded and SHA-256 verified
- [x] embedding_model.onnx downloaded and SHA-256 verified
- [x] hey_jarvis_v0.1.onnx downloaded and SHA-256 verified
- [x] silero_vad.onnx downloaded and SHA-256 verified
- [x] OpenWakeWordDetector 3-model pipeline loads and runs inference
- [x] SileroVad loads and runs inference
- [x] Runtime defaults: OpenWakeWordDetector and SileroVad (not energy-based)

### Pipeline Orchestration

- [x] AudioSessionOrchestrator actor-style design with command channel
- [x] StartFromWake: emits WakeWordDetected + AudioStreamStart, encodes pre-roll + live audio
- [x] StartManual: emits AudioStreamStart, begins capture
- [x] CapturedAudio: Opus-encodes frames, sends as binary WS frames
- [x] VadEndOfSpeech: emits AudioStreamEnd + SpeechEnded
- [x] InboundAudio: Opus-decodes TTS frames
- [x] InboundStreamEnd: handles normal + interrupted (crash-free)
- [x] StopCurrent: clean teardown
- [x] Shutdown: graceful exit

### Command Wiring

- [x] start_listening_session sends StartManual to orchestrator
- [x] stop_current_session sends StopCurrent to orchestrator
- [x] send_text_message sends TextMessage over WS via outbound channel
- [x] WS dispatch routes AudioStreamEnd to orchestrator
- [x] WS dispatch routes binary AudioChunkOut to orchestrator

### Latency Instrumentation

- [x] wake.detected event logged with timing
- [x] stream.start event logged with timing
- [x] stream.first_frame event logged with elapsed_ms
- [x] tts.first_chunk event logged on first inbound audio
- [x] playback.first event logged on decoded audio

## Remaining: Physical Verification

The following require a human with a Mac, microphone, and speaker:

1. Start PS-002 Server: `cd ~/Documents/Github/Pairion-Server && pnpm dev`
2. Start Client: `cd ~/Documents/Github/Pairion-Client && pnpm tauri dev`
3. Wait for "Connected" in the Client window
4. Say "Hey Jarvis" — verify state transitions to Listening
5. Say "What's the weather in Dallas?" — wait for silence → Thinking
6. Verify transcript strip shows partial/final text
7. Verify British-voice TTS plays through speaker
8. Verify state returns to Idle
9. Check console logs for latency measurements (look for `wake.detected`, `stream.start`, `tts.first_chunk`)
10. Record demo video if desired

## Architecture Summary

```
Microphone → cpal → ring buffer → wake detector (ONNX) → orchestrator
                                 → VAD (ONNX) ↗

Orchestrator → Opus encoder → binary WS frames → Server
            ← binary WS frames ← Opus decoder ← playback → Speaker

React UI ← session state (watch channel) ← orchestrator/WS dispatch
```
