# CONVENTIONS.md — Pairion-Client

This document is authoritative for engineering discipline on `Pairion-Client`.
Every contributor (human and AI) reads it before touching code. Every Claude Code prompt assumes these conventions.

---

## 1. Project Standards (Pairion-Wide)

These standards apply across every Pairion repository and are reproduced verbatim in `Pairion-Server/CONVENTIONS.md` and `Pairion-Node/CONVENTIONS.md`.

### 1.1 Development philosophy

This is an AI-first codebase. AI writes 100% of production code. Development velocity is measured in AI-first benchmarks (proven track record of 200K+ lines of code per hour across prior projects). Traditional software development assumptions — sprint timelines, backlogs, phased severity-based deferrals — do not apply.

- **Never phase work by severity or priority.** Every fix, feature, or task is completed in a single pass. There is no cost/time justification for deferral. If it's worth doing, it's done now.
- **Never offer traditional time estimates** in PR descriptions, architecture discussions, or project planning. Effort is a function of prompt iteration, not calendar weeks.
- **No "next sprint" framing.** No "backlog" framing.

### 1.2 Source-of-truth hierarchy

When documents conflict, later beats earlier in this order:

1. `Pairion-Server/openapi.yaml`
2. `Pairion-Server/asyncapi.yaml`
3. `Architecture.md`
4. `Pairion-Charter.md`
5. Any Claude Code prompt
6. Prior conversation context

**Never assume, infer, or guess about a codebase.** Before generating any code, tests, fixes, prompts, or recommendations, verified source-of-truth documents must be consulted. Both Claude (conversational) and Claude Code (filesystem-capable) work from the same verified source — never from memory, conversation context, or inference.

### 1.3 Tests ship with code, always

- **Unit tests and integration tests ship in the same pass as the feature or fix.** No follow-up tickets for tests. Ever.
- **100% code coverage is mandatory, not aspirational.** Enforced by local verification during task execution.
- Tests validate behavior from public API, not internal implementation details.
- Test fixtures and harnesses are first-class code — same conventions, same review bar.

### 1.4 Documentation ships with code, always

- **Every class, every module, every public function has a documentation comment.** (Excluding DTOs, entities, and generated code.)
- TypeScript/JavaScript uses TSDoc/JSDoc. Java uses Javadoc. Dart uses DartDoc. C#/.NET uses XML Doc Comments. Rust uses RustDoc.
- Documentation ships in the same pass as the code. Not a follow-up.
- Auto-generated API docs publish on every release.

### 1.5 Centralized logging

- **All software projects must have centralized logging.**
- Structured, not free-text. Log level, subsystem, correlation id, session/user id where relevant.
- Client and Node logs forward to the Server's centralized log store.
- No `console.log`, no `println!` in production code paths. Use the project's logger.

### 1.6 Database migrations

- **Never use Flyway during development.** Flyway's lifecycle creates significant delays during restart-heavy development cycles.
- Use a lightweight migration runner during development (Hibernate for Java/Spring projects; a TypeScript migration runner for Node projects).
- Flyway adoption happens only when moving to production. Post-v1 concern.
- (Not directly applicable to this repo — the Client has no relational database — but included for consistency across Pairion projects.)

### 1.7 Build tooling

- **Never Gradle. Always Maven** (for Java projects — not applicable to this Rust + TypeScript project, but applies if this convention is adopted for any Java module).
- This project uses **Cargo** for Rust and **pnpm** for JavaScript/TypeScript. No `npm`, no `yarn`, no Gradle, no Bazel in this repo.

### 1.8 Password policy

- **Development passwords:** minimal requirements. Short, simple, memorable. The goal is fast repeated testing without friction.
- **Production passwords:** strong (length, special characters, numbers, etc.). The shift happens when moving to production hardening, not before.

### 1.9 Claude Code prompt format

All prompts to Claude Code are `.md` file artifacts. Every prompt:

- **Begins with a STOP directive** reading the required source-of-truth files:
  > STOP: Before writing ANY code, read these files completely:
  > 1. `~/Documents/Github/Pairion-Server/openapi.yaml` — The OpenAPI spec defines every field name, type, enum value, and endpoint path. Your code must match it exactly.
  > 2. `~/Documents/Github/Pairion-Server/asyncapi.yaml` — The AsyncAPI spec defines the streaming WebSocket protocol.
  > 3. `~/Documents/Github/Pairion-Client/Architecture.md` — The architecture spec defines the component layout, audio pipeline, HUD composition, and invariants.
  > 4. `~/Documents/Github/Pairion-Server/Pairion-Charter.md` — The project charter.
  > Do not rely on the descriptions in this prompt alone. If this prompt conflicts with the source files, the source files win.
  >
  > If any of these files are missing, STOP and ask for them before proceeding.
- **Ends with:** *"Compile, Run, Test, Commit, Push to Github."*
- **Includes a report template** that Claude Code fills in with a summary of work performed, ending with the Git commit hash.

**Prompts do not contain code.** This includes implementation code, test code, configuration snippets, YAML, shell commands, and code examples. Prompts direct Claude Code with goals, constraints, and instructions — never with code. Claude Code has direct filesystem access and must read actual source files before producing any output.

---

## 2. Pairion-Client Stack Conventions

### 2.1 Runtime and tooling

- **Tauri 2** for the cross-platform shell. Produces a signed native `.dmg` on macOS.
- **Rust (stable, latest)** for the Tauri backend. Pinned via `rust-toolchain.toml`.
- **Node.js 24 LTS** for the frontend tooling, matching Server. Pinned via `.nvmrc`.
- **pnpm** for the React dependencies. Lockfile committed.

### 2.2 Project structure

Two halves, one repo:

- **`src-tauri/`** — Rust backend. Real-time and OS-integration responsibilities (audio, screen capture, WebSocket, keychain, computer-use executor, log forwarding).
- **`src/`** — React frontend. Presentation (HUD, cards, settings, enrollment wizard, onboarding).

They communicate via typed **Tauri commands** (React → Rust) and **Tauri events** (Rust → React). No other IPC mechanisms.

### 2.3 Rust backend conventions

- **Tokio** for async. Single runtime for the whole backend.
- **`ort`** (ONNX Runtime Rust) for any on-device model execution (wake-word in Client-initiated sessions).
- **`cpal`** for audio capture and playback. Cross-platform.
- **`tokio-tungstenite`** for WebSocket client. Matches AsyncAPI's binary-frame requirements.
- **`tauri-plugin-keyring`** for secrets.
- **`tracing`** for logging. Configured once in `src-tauri/src/logs/mod.rs`.
- Cargo features are used **sparingly** and only for truly optional platform-specific code (e.g., `macos-screen-capture`). Never for tier-style differential builds.

### 2.4 React frontend conventions

- **TypeScript strict mode.** Root `tsconfig.json` with `"strict": true`, `"noUncheckedIndexedAccess": true`.
- **React 18+** with functional components and hooks. No class components.
- **Tailwind CSS** for styling. No CSS modules, no styled-components, no inline style attribute usage outside of dynamically-computed values.
- **Zustand** for state management. One store per concern (sessionStore, hudStore, cardsStore, settingsStore, connectionStore). Typed actions.
- **Framer Motion** for UI chrome animations (cards sliding, fades, physics).
- **Core HUD animations** draw on `<canvas>` via a single `requestAnimationFrame` loop — React does not re-render per frame. The canvas is the only way to sustain 60fps under dynamic waveform/equalizer data.
- **Shadcn/ui primitives** for buttons, dialogs, form controls in settings and onboarding. Not for the HUD core itself (which is custom canvas).
- **`lucide-react`** for icons.

### 2.5 Audio pipeline discipline

- **Latency is load-bearing.** The audio capture and playback paths have explicit latency budgets in `Architecture.md` §5.3. Regressions fail CI.
- **cpal callbacks must be non-blocking.** No allocations, no locks, no logging in the capture callback. Use ring buffers.
- **Opus encode/decode runs on dedicated threads.** Never on the tokio default executor.
- **Jitter buffers** are tuned per platform; values in source are documented with the reasoning.

### 2.6 Screen capture discipline

**The visible "REC" indicator when screen capture is active is a hard invariant.** Charter §5.6, Client Architecture §7, and this document all require it. CI includes a test that asserts the indicator is rendered whenever a screen capture has occurred within the last 5 seconds. **No PR that weakens or bypasses the indicator is mergeable.** Ever.

### 2.7 Tauri command surface discipline

- Commands exposed to React are **explicitly listed and typed** in `src-tauri/src/commands/`.
- No general-purpose "run arbitrary Rust" or "execute shell" commands. New commands require architectural justification in the PR description.
- Every command has Cargo tests covering its success and failure paths.

### 2.8 Secrets

- Bearer tokens and any adapter API keys are stored in **macOS Keychain** via `tauri-plugin-keyring`. Never in `preferences.json`. Never in logs.
- Tracing output is scrubbed for known secret patterns. The scrubber is tested.
- **Secret leakage tests** run on every PR — log files and event streams are parsed for tokens that shouldn't be there.

### 2.9 Logging

- **Rust:** `tracing` crate with structured fields. Output forwarded to Server via the `LogForward` AsyncAPI message.
- **React:** the `logger` utility in `src/lib/logger.ts` wraps console output with structured fields and forwards to Rust via a Tauri command.
- **`console.log` is banned** in `src/*.ts` and `src/*.tsx`. ESLint rule enforces. `println!` is banned in `src-tauri/src/**`. Custom Cargo clippy lint enforces.

### 2.10 Testing

- **Cargo unit tests** for every public function in `src-tauri/`.
- **Vitest** for React component unit tests.
- **React Testing Library** for behavioral tests on UI interactions.
- **Playwright** for end-to-end against a running test Server.
- **Visual regression tests** — screenshot-diff for key HUD states (idle, listening, thinking, speaking, with-card, offline-indicator). Prevents accidental aesthetic regressions.
- **Audio latency benchmark** — synthetic audio through cpal loopback; CI gate at the budgets in Client Architecture §5.3.
- **Mocked Server fixture** — a lightweight WS test harness so Client tests don't require a real Server.
- **100% coverage on Client public surfaces** (Rust + React).

### 2.11 Documentation

- **RustDoc** on every `pub` item in `src-tauri/`.
- **JSDoc** on every exported React component, hook, and utility function.
- The HUD core animation implementation is heavily commented inline — the rendering invariants are subtle and merit context.

### 2.12 Linting and formatting

- **Rust:** `rustfmt` + `clippy` with `--deny warnings` in CI.
- **TypeScript:** ESLint + Prettier, integrated.
- **Custom lints:** no `console.log` / no `println!` / no arbitrary shell invocation.
- CI fails on lint errors and unformatted code.

### 2.13 Git and PR conventions

- **Trunk-based.** `main` is always green.
- **Conventional Commits:** `feat(hud): add sub-agent spawn animation`, `fix(audio): prevent underrun on wake`.
- **Every PR links its Claude Code prompt.**
- **Local verification must pass before merge.** No GitHub Actions CI is configured. Do not re-add CI workflows unless a prompt explicitly directs it.

### 2.14 Build and packaging

- **`pnpm tauri dev`** for development (React hot-reload + Rust auto-rebuild).
- **`pnpm tauri build`** produces a macOS `.dmg`.
- **Code signing and notarization** are required for the launch build (M12); dev builds ship unsigned.
- **Auto-update** via Tauri's updater — stubbed during development, wired to the release server at M12.

---

## 3. Invariants (from `Architecture.md` §16)

1. **No agent code runs in the Client.** No LLM calls, no memory, no voice-ID logic.
2. **Screen capture shows a visible indicator whenever active.** No exceptions.
3. **The Client never reaches a non-Server network endpoint directly.** Vendor APIs are called by the Server via adapters.
4. **Bearer tokens are keychain-only.** Never in preferences, never in logs.
5. **The HUD sustains 60fps.** Animations that break this are bugs.
6. **Every secret-handling path is tested.** Missing a secret-scrub test blocks merge.
7. **The HUD is photographable in every state.** If a new state is added that looks bad in a screenshot, the state needs design before ship.
8. **Tauri command surface is small.** New commands require architectural justification.
9. **Tests ship with code.**

---

## 4. Local Verification Pipeline

**Verification runs locally during task execution** — Claude Code runs tests, lints, typechecks, and coverage as part of every task. No GitHub Actions CI is configured. Do not re-add CI workflows unless a prompt explicitly directs it.

Every task runs, in order:

1. `pnpm install --frozen-lockfile`
2. `pnpm lint` (ESLint + Prettier check)
3. `cargo fmt -- --check`
4. `cargo clippy --deny warnings`
5. `pnpm typecheck`
6. `pnpm tauri build --debug` (verify builds)
7. `cargo test --workspace`
8. `pnpm test` (Vitest)
9. `pnpm test:e2e` (Playwright against test Server)
10. `pnpm test:visual` (screenshot regression)
11. `pnpm test:leak` (secret leakage tests)
12. `pnpm test:bench` (audio latency)
13. `pnpm coverage:check`

Failure at any step fails the task.

---

## 5. When In Doubt

Read, in order: this document, `Pairion-Charter.md`, `Architecture.md`, `Pairion-Server/openapi.yaml`, `Pairion-Server/asyncapi.yaml`. If still uncertain, the code you're about to write is probably wrong — stop and ask.