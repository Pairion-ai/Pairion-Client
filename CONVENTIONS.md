# Pairion-Client Engineering Conventions

**Version:** 2.2

## 0. User Preferences (Binding — Applied to All Work)

Always be my sparring partner. Look for blind spots in my theories, proposals, ideas, weak arguments, look for missing data. I want the tightest most correct and most winning results and mostly for for you to not be a sycophant. If my idea or suggestion is correct, then strong agreement is warranted and requested. But if not, I need to see alternate ideas, better ideas, better outcomes. Your goal is not to make me happy, but to help me achieve greater accuracy and success in all facets of our interaction.

In addition to the above, for anything relating to software development please additionally apply the following:

I am an AI-first developer producing code at 500x traditional speed. AI writes 100% of production code. Never give traditional time estimates, never phase work by severity/priority, never suggest "next sprint" or "backlog." Every fix, feature, or task is completed in a single pass — there is no cost/time justification for deferral. When estimating effort, use proven AI-first benchmarks: we've done over 200k lines of code per hour. Traditional software development assumptions do not apply to my workflow.

NEVER assume, infer, or guess about any codebase. You have no filesystem access. Before generating any code, tests, fixes, prompts, or recommendations that touch a codebase, you MUST request: (1) A current comprehensive audit that was produced using our Claude Audit Template for every project/codebase involved, and (2) The OpenAPI.yaml (also created by the audit template) for every service involved, so you understand the full REST API surface. If the work touches multiple projects, request audits and OpenAPI specs for ALL of them — never leave any out. Do not proceed until you have these. When I provide these files, also ask for their filesystem paths so you can reference them in any Claude Code prompts you generate. Both you and Claude Code must work from the same verified source of truth — never from memory, conversation context, or inference.

All code — features, fixes, remediations, anything — ships with tests in the same pass. 100% code coverage is mandatory, not aspirational. This includes both unit and integration tests. All tests are never a follow-up task. We never use Flyway during development. Flyway can cause significant delays when stopping and restarting services, which is typical during development. We only use Flyway when we move a project into production. Before that, we use Hibernate. Never use Gradle, we ALWAYS use Maven. Likewise, for production we would normally require strong passwords (length, special characters, numbers, etc), but during development, when repeatedly testing, we want minimal requirements to make logins fast and easy.

All code must have documentation comments on every class/module and every public method/function (excluding DTOs, entities, and generated code). Java uses Javadoc, TypeScript/JavaScript uses TSDoc/JSDoc, Dart uses DartDoc, C#/.NET uses XML Doc Comments. C++ uses Doxygen-style comments. Documentation ships in the same pass as the code. All software projects must have centralized logging.

All prompts to Claude Code must be .md file artifacts. Every prompt must begin with: "STOP: Before writing ANY code, read these files completely: 1. ~/Documents/Github/<project folder>/openapi.yaml — The OpenAPI spec defines every field name, type, enum value, and endpoint path. Your code must match it exactly. 2. ~/Documents/Github/<project folder>/<project name>-Audit.md — The server audit shows actual entity relationships, repository methods, and validation rules. 3. ~/Documents/Github/<project folder>/<project name>-Architecture.md — The architecture spec defines all routes, widgets, services, and the project structure. Do not rely on the descriptions in this prompt alone. If this prompt conflicts with the source files, the source files win." If any of these files are missing, STOP, and ask for them before proceeding.

Each prompt must end with: "Compile, Run, Test, Commit, Push to Github". Your prompts will also end with a template that Claude Code will use as a report format to give back the summary of the work it performed during the prompt. That report template will include the Git commit hash.

Claude never writes code of any kind in any prompt. This includes implementation code, test code, configuration snippets, YAML, shell commands, and code examples. Prompts direct Claude Code with goals, constraints, and instructions — never with code. Claude Code has direct filesystem access and must read actual source files before producing any output. If achieving a goal requires code, the prompt tells Claude Code what to accomplish and what files to read, not how to write it.

---

## 1. Repository Stack

- **Language:** C++ 20
- **Framework:** Qt 6.6+ (Qt Core, Qt Quick, Qt Multimedia, Qt WebSockets, Qt Network, Qt Test)
- **Build system:** CMake 3.25+, using Qt6's CMake integration and cross-platform CMake presets
- **Dependency manager:** Conan or vcpkg for third-party C++ dependencies (onnxruntime, libopus). Qt itself managed via `aqtinstall` or the official Qt online installer.
- **Test framework:** Qt Test (`QtTest` module) for unit tests; mock WebSocket server for integration tests
- **Coverage:** gcov on Linux/macOS, OpenCppCoverage on Windows. 100% line coverage enforced locally via CMake coverage target.
- **Shader toolchain:** `qsb` (Qt shader baker) compiles GLSL source to cross-platform `.qsb` files that run on Metal, Vulkan, and D3D11 backends.

## 2. Documentation Requirements

- **Doxygen-style comments** on every public class, member function, free function, and type alias (excluding POD DTOs)
- Every header file has a top-of-file comment describing the module's purpose
- Class-level comments explain responsibility, threading model, and ownership
- `README.md` at repo root with platform setup, build, run, test instructions
- `Architecture.md` at repo root (source of truth for architecture)
- `CHANGELOG.md` at repo root, Keep A Changelog format

## 3. Coverage

- 100% line coverage required on everything in `src/` and `test/`
- Enforced locally via CMake target that fails if coverage < 100%
- Tests cover: units, QML widget behavior via `QtQuickTest`, integration paths against mock WebSocket harness

## 4. Local Verification (no CI during development)

Every task runs and must pass before commit:
- `cmake --preset macos-debug` (or platform preset) — configure
- `cmake --build --preset macos-debug` — compile, zero warnings with `-Wall -Wextra -Wpedantic -Werror` on Linux/macOS; `/W4 /WX` on Windows
- `ctest --preset macos-debug --output-on-failure` — runs all tests
- `cmake --build --preset macos-debug --target coverage` — coverage check (fails if < 100%)
- `clang-format --dry-run --Werror **/*.cpp **/*.h` — formatting

## 5. Code Style

- Header guards via `#pragma once`
- `clang-format` enforced (config at repo root; Qt-style based with project-specific tweaks)
- No `using namespace std;` at file scope
- No `using namespace Qt;` at file scope
- Prefer `std::` containers over `QList`/`QMap` for non-Qt-facing code; Qt containers where they interface with Qt signals/slots/models
- Smart pointers (`std::unique_ptr`, `std::shared_ptr`) for ownership; raw pointers only for non-owning QObject parent/child relationships
- `const` by default; `constexpr` where applicable
- Sealed variant types (`std::variant` with visitor pattern) for closed message hierarchies

## 6. QML Style

- QML files named `PascalCase.qml` for components, `camelCase.qml` for root files
- Each QML file has one top-level type declaration
- Inline JavaScript kept minimal — logic goes in C++ backend objects
- Animation via `Behavior`, `NumberAnimation`, `SequentialAnimation`
- Shaders loaded from `.qsb` files (pre-compiled), never as inline GLSL strings

## 7. Threading

- Main thread owns QML, Qt scene graph, widgets
- Audio capture, playback, Opus encode/decode, ONNX inference all run on worker threads
- Thread-safety via signals/slots with `Qt::QueuedConnection` / `Qt::AutoConnection`
- No raw mutex usage outside low-level ring buffers; prefer Qt concurrency primitives

## 8. Memory and Lifetime

- QObjects use Qt's parent-child lifetime model
- Non-QObject resources use RAII via smart pointers
- No `new` without a corresponding `delete` or smart pointer wrapper
- No `malloc` / `free` in application code

## 9. Secrets

- Device bearer token stored via `QSettings` (standard Qt persistence).
- QtKeychain evaluated and removed during M0 due to incompatibility with GUI-less Qt Test contexts. Development-phase trade-off accepted per §0 user preference for minimal development-phase friction.
- Production hardening of secret storage is an M12 launch-polish concern.
- No API keys on the Client.

## 10. Logging

- All logging via `qDebug`, `qInfo`, `qWarning`, `qCritical` with `QLoggingCategory` categories
- Custom message handler installed via `qInstallMessageHandler` formats as structured JSON and forwards to Server
- No `std::cout` / `std::cerr` in production code paths (enforced by lint)
- No `printf` / `fprintf` in production code paths

## 11. Platform Setup

- **macOS:** `NSMicrophoneUsageDescription` in `Info.plist`. App sandbox entitlements for audio + network. Universal binary (x86_64 + arm64) via CMake.
- **Windows:** MSVC 2022 toolchain. Microphone permission handled by OS. Installer via Qt Installer Framework or NSIS.
- **Linux:** GCC 13+ or Clang 16+. PulseAudio/PipeWire abstracted by Qt Multimedia. AppImage packaging preferred.

## 12. Commits

- Conventional Commits
- Every commit compiles clean, tests pass, zero warnings
- Never commit broken code

## 13. Prompts to Claude Code

- Every prompt is a `.md` file artifact
- STOP directive at top (references `openapi.yaml`, `Audit.md` when applicable, `Architecture.md`)
- "Compile, Run, Test, Commit, Push to Github" at bottom
- Report template at bottom (includes Git commit hash)
- Prompts never contain code