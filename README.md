# Pairion Client

> The ambient AI HUD for macOS — the face of [Pairion](https://pairion.ai).

<!-- TODO: Replace with demo video/GIF after M2 -->
![Demo Placeholder](https://img.shields.io/badge/demo-coming%20soon-amber)

## What is Pairion?

Pairion is an ambient, voice-first AI presence that lives throughout a household. The Client is the per-user HUD running on each household member's Mac — a borderless, cinematic interface that shows what Pairion is doing and lets you interact.

See the [Pairion Charter](https://github.com/your-org/Pairion-Server/blob/main/Pairion-Charter.md) for the full vision.

## Development Setup

### Prerequisites

- **macOS 14+** (Apple Silicon)
- **Rust** (stable, via [rustup](https://rustup.rs))
- **Node.js 24** (via [nvm](https://github.com/nvm-sh/nvm) or [fnm](https://github.com/Schniz/fnm))
- **pnpm** (`npm install -g pnpm`)
- **Tauri CLI** (`cargo install tauri-cli --version "^2"`)

### Install & Run

```bash
# Clone
git clone https://github.com/your-org/Pairion-Client.git
cd Pairion-Client

# Install frontend dependencies
pnpm install

# Run in development mode (React hot-reload + Rust auto-rebuild)
pnpm tauri dev

# Build a debug .dmg
pnpm tauri build --debug
```

### Running Tests

```bash
# Rust tests
cargo test --workspace --manifest-path src-tauri/Cargo.toml

# React tests
pnpm test

# Lint
pnpm lint
cargo fmt -- --check
cargo clippy --deny warnings --manifest-path src-tauri/Cargo.toml

# Type check
pnpm typecheck
```

## Architecture

See [Pairion-Client-Architecture.md](./Pairion-Client-Architecture.md) for the full architecture specification.

## Current Status: M0 — Walking Skeleton

The Client connects to a running Pairion Server via WebSocket, shows connection status, and demonstrates the full build/test/lint discipline. No audio, no HUD animations, no features beyond connect/disconnect.

## License

Source-available, non-commercial. See [LICENSE](./LICENSE).
