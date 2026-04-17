# Pairion-Client — Architecture

**Version:** 1.0
**Status:** Normative. Source of truth for how `Pairion-Client` is internally organized. Claude Code prompts that touch this repo read this document alongside `openapi.yaml` and `asyncapi.yaml`.
**Derives from:** `Pairion-Charter.md` v1.2, `openapi.yaml` v1.0.0-alpha.3, `asyncapi.yaml` v1.0.0-alpha.1.

---

## 1. Purpose and Scope

Pairion-Client is the **per-user HUD** running on a household member's Mac. It is the face of Pairion for that person — the cinematic, always-there, photographable presence that earns screenshots. It handles mic capture and playback, renders the HUD's animated visual states, surfaces proactive cards and approval flows, and talks to the Server over a single WebSocket per Client.

The Client is **not** the brain. No agent runs here. No memory, no voice-ID, no LLM, no skills. All reasoning happens on the Server. The Client is a surface — an instrument — not a mind.

---

## 2. Architectural Overview

```
┌────────────────────────────── PAIRION-CLIENT ───────────────────────────────┐
│                                                                              │
│   ┌──────────────────────────┐        ┌──────────────────────────────────┐  │
│   │  REACT FRONTEND           │        │  RUST BACKEND (Tauri main)       │  │
│   │  (HUD)                    │        │                                   │  │
│   │                           │◀──IPC─▶│  • WebSocket client              │  │
│   │  • React + TS + Tailwind  │        │  • Audio pipeline (cpal)         │  │
│   │  • Framer Motion          │        │  • Screen capture (SCKit)        │  │
│   │  • Zustand state          │        │  • Computer-use dispatcher       │  │
│   │  • Canvas/SVG animations  │        │  • Keychain secrets              │  │
│   │  • Shadcn/ui primitives   │        │  • OS integration (notifs, tray) │  │
│   │  • Sound-design library   │        │  • Log forwarding to Server      │  │
│   └──────────────────────────┘        └──────────────────────────────────┘  │
│                                                 │                            │
│                                                 │ WebSocket (Opus audio       │
│                                                 ▼  + JSON events)            │
│                                       ┌────────────────────┐                 │
│                                       │   PAIRION-SERVER    │                 │
│                                       └────────────────────┘                 │
└──────────────────────────────────────────────────────────────────────────────┘
```

The split: Rust owns **real-time and OS-integration** responsibilities (audio, screen, WebSocket, keychain, computer-use). React owns **presentation** (HUD, cards, settings). IPC between them is typed via Tauri commands and events.

---

## 3. Runtime Model

- **Single Tauri 2 process.** One main Rust binary with a WebView rendering the React app. No separate helper processes in v1.
- **WebView:** system WebKit on macOS. Handles the cinematic HUD (glassmorphism, reactive canvas) at 60fps.
- **Main window:** borderless, transparent, configurable always-on-top. This is the HUD itself. There is no traditional window chrome.
- **Secondary windows:** spawned on demand for settings, enrollment wizard, Node pairing flow, onboarding.
- **System tray icon:** present; minimal menu for quit / toggle HUD visibility / switch user / preferences. Tray is a fallback surface when the HUD is hidden.
- **Startup:** Client loads keychain → bearer token → opens WebSocket → identifies → receives initial state → renders HUD.

---

## 4. Project Layout

```
Pairion-Client/
├── src-tauri/                    # Rust backend
│   ├── src/
│   │   ├── main.rs
│   │   ├── commands/             # Tauri commands exposed to React
│   │   ├── audio/                # cpal capture + playback, Opus codec
│   │   ├── ws/                   # WebSocket client, reconnect, message dispatch
│   │   ├── screen/               # ScreenCaptureKit integration
│   │   ├── secrets/              # Keychain wrapper
│   │   ├── computer_use/         # Action executor for approved actions
│   │   ├── logs/                 # tracing + log forwarding to Server
│   │   └── state/                # Shared state (tokio::sync::RwLock)
│   ├── Cargo.toml
│   └── tauri.conf.json
├── src/                          # React frontend
│   ├── hud/                      # HUD components and animations
│   ├── cards/                    # Approval, proactive, guest-request cards
│   ├── settings/                 # Settings UI
│   ├── enrollment/               # Voice enrollment wizard
│   ├── onboarding/               # First-run setup
│   ├── state/                    # Zustand stores
│   ├── lib/                      # Shared utilities, tauri IPC wrappers
│   └── App.tsx
├── tests/
│   ├── rust/                     # Cargo tests
│   ├── react/                    # Vitest + React Testing Library
│   └── e2e/                      # Playwright (against test Server)
├── CONVENTIONS.md
├── README.md
├── package.json
└── pnpm-workspace.yaml (single package; monorepo-ready)
```

Every Rust module has RustDoc. Every React component has JSDoc. Both sides target 100% coverage on public surfaces.

---

## 5. Audio Pipeline

This is the Client's primary real-time responsibility. Latency here is load-bearing for the 700ms round-trip budget.

### 5.1 Capture path

`mic → cpal → PCM → VAD (silero, optional at Client) → Opus encoder → WebSocket frame → Server`

- **cpal** for cross-platform mic access (macOS via CoreAudio).
- **Ring buffer** between cpal callback and Opus encoder thread to keep capture non-blocking.
- **Opus encoder** (libopus via `opus` crate), 48kHz mono, variable bitrate, 20ms frame size.
- **AudioStreamStart** envelope sent as JSON before the first binary frame; includes stream id, codec, sample rate, direction.
- **AudioChunkIn** binary frames sent as fast as they're produced; one frame per Opus frame.
- **AudioStreamEnd** sent when VAD determines speech ended (or when Client decides to cap input length).
- **SpeechEnded** AsyncAPI message sent alongside stream end so Server can finalize STT.

Wake word detection runs Client-side on this audio stream for Client-initiated sessions (openWakeWord). When wake fires, Client emits `WakeWordDetected`, then starts streaming audio. Server confirms with `SessionOpened`.

### 5.2 Playback path

`WebSocket → AudioChunkOut binary frame → Opus decoder → jitter buffer → cpal output → speaker`

- **Opus decoder** (libopus) matching the encoder.
- **Jitter buffer** (small, 40–60ms target) to absorb network variance.
- **cpal output** streams decoded PCM to the OS audio output.
- **AudioStreamEnd** with `reason: interrupted` from Server triggers immediate playback cancellation and buffer flush (< 50ms target).
- **Under-breath acks** are NOT streamed TTS — they arrive as `UnderBreathAck` messages with a `variant` id; Client plays the corresponding sample from its local sound-design cache.

### 5.3 Latency budget

| Stage | Budget |
|---|---|
| Mic → first Opus frame on wire | < 40ms |
| First Opus frame receive → playback | < 80ms |
| WakeWordDetected emit → Server receive | < 10ms LAN |
| Interrupt detect → TTS audio stops | < 150ms (barge-in pillar) |
| Under-breath ack trigger → audio out | < 100ms |

These are all CI-benchmarked on macOS reference hardware.

---

## 6. HUD Composition

The HUD is the Pairion brand made visible. Its rendering quality is the difference between "cool tech demo" and "people take screenshots."

### 6.1 Layout

The HUD is a borderless, translucent window rendered over the user's desktop. Position is user-configurable; default is lower-right corner.

Compositionally:

- **Core circle** — the primary animated element. Idle: subtle breathing glow. Listening: reactive waveform. Thinking: arc-reactor pulse. Speaking: equalizer.
- **Telemetry readouts** — hairline monospace text surrounding the core, rendering agent state, current speaker chip, latency indicators.
- **Card stack** — approval/proactive/info cards slide in from the right and dismiss left.
- **Transcript strip** — optional low-opacity live transcript below the core (on by default; toggleable).
- **Corner widgets** — settings gear (top-right), minimize (top-left), voice profile switcher (bottom-left).

### 6.2 Rendering

- **Core animations:** `<canvas>` element driven by a single `requestAnimationFrame` loop. All four core states share one loop; the state determines which draw function runs that frame. This gives us determinate 60fps without React reconciliation overhead on the hot path.
- **Reactive waveform:** driven by `HudStateUpdate.waveformData` from the Server, smoothed client-side for continuity between updates.
- **Arc-reactor pulse:** driven by `HudStateUpdate.pulseRateHz`; LFO-style oscillator in JS.
- **Equalizer:** driven by TTS amplitude computed client-side from the decoded PCM (for tight sync with audible speech).
- **Cards and UI chrome:** React + Framer Motion for physics-y transitions. React state via Zustand; components are purely presentational.
- **Typography:** monospace for telemetry (JetBrains Mono or similar permissive-license font); humanist sans for card body copy (Inter or similar).
- **Color:** amber/cyan primary palette with muted purples for handoff, teals for offline (Smart Node surfacing). Dark-mode-native; light mode is a post-v1 concern.

### 6.3 State management

- **Zustand** chosen over Redux/Context. Lightweight, typed, enough for our scope.
- **Primary stores:**
  - `sessionStore` — current Session state, identified User, turn state
  - `hudStore` — animation state, waveform data, telemetry readouts
  - `cardsStore` — pending and displayed cards
  - `settingsStore` — user preferences for HUD position, transcript visibility, etc.
  - `connectionStore` — WebSocket health, reconnect state
- Each store exposes typed actions; the WS dispatcher calls these actions on incoming messages.

### 6.4 The cold-start ritual

On first launch (or when explicitly triggered from settings), the Client plays the cold-start ritual (Charter Pillar 5.1). This is NOT a Rust-driven subsystem — it's a React-driven choreographed sequence that consumes subsystem-readiness events from the Server and theatrically reveals the HUD over 20–30s. Skippable after first run. Always available from `Settings → Replay Cold Start`.

---

## 7. Screen Capture Pipeline

For the "what am I looking at" and computer-use features (Charter Pillar 5.6).

- **macOS:** `ScreenCaptureKit` via a thin Rust wrapper.
- **Permissions:** first screen capture request triggers the macOS system permission dialog. Permission is stored and respected thereafter.
- **Visible indicator:** whenever screen capture is active (a frame has been captured in the last 5 seconds), the HUD shows an unmistakable red "REC" dot. **This is a hard invariant — screen capture cannot happen without this indicator.** If the HUD is hidden, a system notification fires on first capture per session.
- **Capture scope:** active display only in v1 (multi-display handling post-v1). Full screen, not app-scoped.
- **Transport:** captured frame is sent to Server as a base64 JPEG over the REST endpoint `/v1/screen/capture` (not WS — one-shot, not streaming). Server routes to the active vision LLM adapter.

---

## 8. Computer-Use Executor

For approved actions arriving via `ActionPending` + approved `ActionDecision` (Charter Pillar 5.6).

- **Executor runs in Rust.** Actions are typed: `click`, `type`, `keystroke`, `script`, `file-edit`, etc.
- **Sandbox:** file-edit actions operate only under directories the user has pre-approved in settings. An attempt to edit outside these directories fails loudly and notifies.
- **Shell actions:** not enabled by default; require explicit user opt-in per session.
- **Approval flow:** Server sends `ActionPending`, HUD displays the approval card, user clicks approve/reject, Client sends `ActionDecision`, Server dispatches the action to the executor, executor runs, Client emits `ActionExecuted`.
- **Timeouts:** per-action timeout (default 60s) beyond which the action auto-rejects and the card dismisses.

---

## 9. Tauri Command Surface

React calls into Rust via typed Tauri commands. The surface is small and purposeful — no general-purpose "run arbitrary Rust" escape hatch.

Commands (representative, not exhaustive; full list lives in `src-tauri/src/commands/`):

- `identify_device()` → sends DeviceIdentify over WS, returns the IdentifyAck
- `start_listening_session()` / `stop_current_session()` — for user-initiated sessions
- `send_text_message(sessionId, text)`
- `approve_action(actionId)` / `reject_action(actionId)`
- `enroll_sample(userId, audioBuffer)` — streams enrollment audio via WS
- `request_screen_capture(sessionId)` — triggers capture + send
- `get_connection_state()` / subscribe to connection state events
- `set_hud_position(x, y)` / `set_hud_always_on_top(bool)` / `set_transcript_visible(bool)`
- `get_user_me()` / `list_devices()` — thin wrappers over REST

**Events from Rust to React** flow via Tauri's event system:

- `ws.message` — forwarded AsyncAPI messages
- `audio.vad.speaking` / `audio.vad.silent`
- `screen.capture.active` / `screen.capture.inactive`
- `connection.state_change`
- `action.execution.progress`

React subscribes via `@tauri-apps/api/event`.

---

## 10. WebSocket Management

- **Single WS connection** per Client lifetime (reconnecting as needed).
- **Rust side owns the WS.** React never touches the WS directly; it interacts via commands + events.
- **Reconnection:** exponential backoff (1s, 2s, 4s, 8s, max 30s). During reconnection, a `connection.state_change` event fires; HUD shows a "reconnecting" visual state.
- **Heartbeats:** send `HeartbeatPing` every 15s; if no `HeartbeatPong` for 30s, tear down and reconnect.
- **Message dispatch:** incoming messages are routed by `type` to typed handlers. Binary frames (Opus audio) go to the audio pipeline directly; JSON messages go to the appropriate state updater.
- **Outbound backpressure:** audio frames take priority; if the send queue is backing up, non-essential messages (log forwards, heartbeats) are dropped first.

---

## 11. Secrets and Settings

- **Bearer token** stored in macOS Keychain via `tauri-plugin-keyring`. Retrieved on startup; never logged.
- **User preferences** stored as JSON in `~/Library/Application Support/Pairion/preferences.json`.
- **Server URL and port** stored in preferences; default `ws://localhost:18789/ws/v1`.
- **Onboarding flow** writes initial preferences including HUD position, proactive settings, quiet hours, and profile selection.

No secret is ever written to preferences. No secret is ever logged. Tracing output is scrubbed for known secret patterns (bearer tokens, API keys that might accidentally appear).

---

## 12. Logging

- **tracing** crate for structured Rust logs.
- **console.log is banned** in React; use the `logger` utility in `src/lib/logger.ts` which wraps `console` with structured fields and forwards to Rust which forwards to Server.
- **Log forwarding:** Rust batches log entries and sends `LogForward` messages over WS to the Server every few seconds (or when a critical log fires). Server indexes these into the centralized log store.
- **Local log file:** `~/Library/Logs/Pairion/client.log` rotated daily. Used when Server is unreachable so logs are not lost.

---

## 13. Testing Architecture

- **Rust unit tests (Cargo)** for every public function in `src-tauri/`.
- **React unit tests (Vitest)** for components and hooks.
- **React Testing Library** for behavioral tests on UI interactions.
- **Playwright** for end-to-end tests against a running test Server.
- **Visual regression:** screenshot-diff tests for key HUD states (idle, listening, thinking, speaking, with-card, offline-indicator). Catches accidental aesthetic regressions.
- **Audio latency benchmark:** synthetic audio injected through the cpal capture loopback; measures end-to-end pipeline latency. CI gate at 700ms.
- **Mocked Server fixture:** a lightweight test harness that implements enough of the WS protocol for Client tests to run without a real Server.

100% coverage on Client public surfaces (Rust + React). Enforced by CI.

---

## 14. Build and Packaging

- **`pnpm tauri dev`** for development (React hot-reload + Rust auto-rebuild).
- **`pnpm tauri build`** produces a signed `.dmg` for macOS.
- **Code signing:** ships as an unsigned build in v1 dev cycle; signing with an Apple Developer ID is a pre-launch task for M12.
- **Notarization:** required for macOS 15+ smooth install; stubbed for dev, real for launch.
- **Auto-update:** Tauri updater stubbed in M0; wired to a real update server at M12.

The install script (`curl -fsSL install.pairion.ai | bash`) on macOS:

1. Downloads the latest `.dmg`
2. Mounts and copies `Pairion.app` to `/Applications/`
3. Opens the app
4. App detects first-run, prompts Server setup if Server not already present
5. After Server runs, Client auto-pairs

---

## 15. Accessibility

- **Keyboard-only operation** for all settings and card interactions
- **Screen reader labels** on every interactive HUD element
- **Reduced motion** preference respected — cold-start ritual skips, animations simplify
- **Transcript strip** provides visual parity with spoken content for users who prefer or need it

Accessibility is a ship-blocker, not a nice-to-have. Charter §11 quality attributes include it.

---

## 16. Invariants (Non-Negotiable)

1. **No agent code runs in the Client.** No LLM calls, no memory, no voice-ID logic. The Client sends audio and events; the Server decides.
2. **Screen capture shows a visible indicator whenever active.** No exceptions.
3. **The Client never reaches out to a non-Server network endpoint directly.** Vendor APIs are called by the Server via adapters.
4. **Bearer tokens are keychain-only.** Never in preferences, never in logs, never in memory longer than needed.
5. **The HUD sustains 60fps.** Animations that break this are bugs.
6. **Every secret-handling path is tested.** Missing a secret scrub test blocks merge.
7. **The HUD is photographable in every state.** If a new state is added that looks bad in a screenshot, the state needs design before ship.
8. **Tauri command surface is small.** New commands require architectural justification.
9. **Tests ship with code.** No exceptions, no follow-up tickets.

---

**This document is the Client's spec. It evolves as decisions surface during implementation, but it never trails implementation — if a decision gets made, this document is updated in the same PR that makes the decision.**
