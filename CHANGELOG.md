# Changelog

All notable changes to Pairion Client will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

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
