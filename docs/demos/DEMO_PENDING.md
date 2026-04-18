# M1 First Voice Demo — Pending

This task was executed from a terminal context (Claude Code) without access to
audio hardware (microphone, speaker) or a GUI window system. The end-to-end
demo requires physical human interaction with audio I/O hardware, which is
impossible from this execution context.

## What Was Verified Programmatically

- All 4 ONNX models download, verify SHA-256, and load successfully
- openWakeWord 3-model pipeline runs inference on silence without crash
- Silero VAD runs inference on silence and correctly reports non-speech
- Opus codec encodes and decodes audio (round-trip test passes)
- All 145 Rust tests + 55 React tests pass
- cargo fmt, clippy (0 warnings), build all pass
- pnpm lint, typecheck, test all pass

## What Requires Physical Verification

The following must be tested by a human at the keyboard with a working
microphone and speaker:

1. Start PS-002 Server: `cd ~/Documents/Github/Pairion-Server && pnpm dev`
2. Start Client: `cd ~/Documents/Github/Pairion-Client && pnpm tauri dev`
3. Wait for "Connected" status
4. Say "Hey Jarvis" into the Mac's default microphone
5. Observe state transition to "Listening"
6. Say "What's the weather in Dallas?"
7. Wait for silence → "Thinking" transition
8. Observe transcript strip showing partial/final text
9. Listen for British-voice TTS response through speaker
10. Note measured wake-to-first-TTS-byte latency from console logs

## Latency Measurement

Latency cannot be measured from this execution context because it requires:
- A physical microphone capturing spoken audio
- The cpal audio pipeline producing real PCM samples
- The wake-word detector processing real speech
- Network round-trip to the running Server
- TTS audio streaming back and playing through a physical speaker

The tracing instrumentation is in place to log the latency at `info` level
when the pipeline runs. Look for log entries containing "wake-to-first-tts"
or timing measurements in the `ws` and `audio` subsystems.

## Recording Instructions

1. Open QuickTime Player → File → New Screen Recording
2. Include audio capture (click dropdown arrow next to record button)
3. Start recording
4. Perform steps 1-10 above
5. Stop recording
6. Save as `m1-first-voice.mov` in this directory

## Why This Cannot Be Automated

Claude Code runs as a non-interactive CLI process without access to:
- CoreAudio input devices (microphone)
- CoreAudio output devices (speaker)
- A Tauri/WebKit window system
- Screen recording capability

This is an inherent limitation of the execution context, not a code issue.
The audio pipeline, ONNX models, and WebSocket integration are fully
implemented and tested programmatically.
