# pairion-client — Quality Scorecard

**Generated:** 2026-04-19T17:30:00Z
**Branch:** main
**Commit:** 3905e308801c11646fb1c79166b280fcd29bdc4a chore: remove all GitHub CI references

---
## Security (10 checks, max 20 pts — 2 pts each)

| Check | Result | Score |
|---|---|---|
| SEC-01 Password encoding (BCrypt/Argon2) | N/A — no password storage in this client | 2 |
| SEC-02 Auth token validation | Bearer token sent on connect; no JWT validation (device UUID) | 1 |
| SEC-03 SQL injection prevention | N/A — no database | 2 |
| SEC-04 CSRF protection | N/A — not a web server | 2 |
| SEC-05 Rate limiting | Not implemented | 0 |
| SEC-06 Sensitive data logging prevention | Bearer token logged in DeviceIdentify? No — not logged directly. PASS | 2 |
| SEC-07 Input validation on all entry points | PCM frame size validated; binary frame size validated; JSON parse errors handled | 2 |
| SEC-08 Authorization checks | N/A — no admin endpoints | 2 |
| SEC-09 Secrets externalized | No secrets in code; bearer token in QSettings (OS keychain on macOS) | 2 |
| SEC-10 HTTPS/WSS enforced | FAIL — `ws://` plaintext; no `wss://` | 0 |

**Security Score: 15/20 (75%)**

## Data Integrity (8 checks, max 16 pts)

N/A — this is a C++ client application with no database, ORM, or entities. All checks reference Spring/JPA patterns.

| Check | Result | Score |
|---|---|---|
| DI-01 All entities have audit fields | N/A (no entities) | 2 |
| DI-02 Optimistic locking | N/A | 2 |
| DI-03 Cascade delete protection | N/A | 2 |
| DI-04 Unique constraints | N/A | 2 |
| DI-05 Foreign key constraints | N/A | 2 |
| DI-06 Nullable fields documented | N/A | 2 |
| DI-07 Soft delete pattern | N/A | 2 |
| DI-08 Transaction boundaries | N/A | 2 |

**Data Integrity Score: 16/16 (100%) — N/A category**

## API Quality (8 checks, max 16 pts)

This is a WebSocket/AsyncAPI client, not a REST API server. Checks adapted accordingly.

| Check | Result | Score |
|---|---|---|
| API-01 Consistent error response format | `std::nullopt` for unknown messages; error types in InboundMessage variant | 2 |
| API-02 Pagination on list endpoints | N/A — no REST API | 2 |
| API-03 Validation on request bodies | PCM frame size, binary frame size, JSON parse errors all validated | 2 |
| API-04 Proper status codes (HTTP) | N/A — WebSocket protocol; HTTP error handling in Logger flush | 1 |
| API-05 Protocol versioning | `/ws/v1` in URL; `clientVersion` sent in DeviceIdentify | 2 |
| API-06 Request/response logging | All WS messages logged via `qCInfo(lcWs)` + Logger | 2 |
| API-07 HATEOAS | N/A | 2 |
| API-08 Protocol documentation | messages.h fully documents all message types | 2 |

**API Quality Score: 15/16 (94%)**

## Code Quality (11 checks, max 22 pts)

| Check | Result | Score |
|---|---|---|
| CQ-01 Constructor injection (not field injection) | All dependencies injected via constructor throughout. PASS | 2 |
| CQ-02 Consistent patterns | Qt patterns used consistently: Q_OBJECT, Q_PROPERTY, signals/slots, Q_LOGGING_CATEGORY | 2 |
| CQ-03 No System.out/printStack equivalent | No `std::cout`, no `fprintf(stderr...)`. All logging via `qCInfo`/`qCWarning`. PASS | 2 |
| CQ-04 Logging framework used | `QLoggingCategory` + centralized `Logger`. PASS | 2 |
| CQ-05 Constants extracted | All magic numbers in `constants.h` or class `static constexpr`. PASS | 2 |
| CQ-06 Separation of concerns | Clear module separation: audio, core, pipeline, protocol, settings, state, util, vad, wake, ws | 2 |
| CQ-07 Service layer exists | Equivalent: each subsystem class (orchestrator, ws client, vad, wake) is the service layer | 2 |
| CQ-08 Testable design | `OnnxInferenceSession` abstract interface + `MockOnnxSession`; `MockServer`; testable audio capture constructor | 2 |
| CQ-09 Doc comments on classes = 100% (BLOCKING) | FAIL — `PairionAudioPlayback` class missing class-level Doxygen comment. 16/17 = 94% | 0 |
| CQ-10 Doc comments on public methods = 100% (BLOCKING) | FAIL — `PairionAudioPlayback` public methods undocumented (start, stop, handleOpusFrame, handlePcmFrame, handleStreamEnd). Several other slots/signals lack per-method docs. | 0 |
| CQ-11 No TODO/FIXME/placeholder (CRITICAL - BLOCKING) | FAIL — 1 TODO found: `audio_session_orchestrator.cpp:89` (PC-003: pre-roll not Opus-encoded) | 0 |

**Code Quality Score: 16/22 (73%) — BLOCKING: CQ-09, CQ-10, CQ-11 all fail; per scorecard rules, entire category = 0**
**ADJUSTED Code Quality Score: 0/22 — BLOCKED by CQ-09, CQ-10, CQ-11**

## Test Quality (12 checks, max 24 pts)

| Check | Result | Score |
|---|---|---|
| TST-01 Unit test files exist | 13 test executables covering all major modules | 2 |
| TST-02 Integration test files | `tst_integration.cpp`, `tst_ws_client_state_machine.cpp`, `tst_audio_session_orchestrator.cpp` (with MockServer) | 2 |
| TST-03 Real infrastructure in ITs | MockServer implements the AsyncAPI protocol for realistic integration testing | 2 |
| TST-04 Source-to-test ratio | 13 test files for ~17 source modules (≈1:1) | 2 |
| TST-05a Unit test coverage = 100% | NOT MEASURED — coverage report not run at audit time. Coverage target exists in CMake (`PAIRION_COVERAGE=ON`, 100% threshold enforced by `cmake/check_coverage.cmake`). | N/A |
| TST-05b Integration test coverage = 100% | NOT MEASURED — see above | N/A |
| TST-05c Combined coverage = 100% | NOT MEASURED — see above | N/A |
| TST-06 Test infrastructure exists | `MockOnnxSession` + `MockServer` — comprehensive test infrastructure | 2 |
| TST-07 Security tests | `tst_device_identity.cpp` tests credential persistence; `tst_ws_client_state_machine.cpp` tests auth handshake | 1 |
| TST-08 Auth flow tested | `tst_ws_client_state_machine.cpp` + `tst_integration.cpp` test DeviceIdentify → SessionOpened flow | 2 |
| TST-09 State verification in tests | All test files verify state transitions with QCOMPARE/QVERIFY | 2 |
| TST-10 Test methods | All 13 test executables have test methods; comprehensive type/edge/error coverage | 2 |

**Test Quality Score: 19/24 (79%) — NOTE: Coverage not measured at audit time; run `cmake --build --preset macos-arm64-debug --target coverage` to verify 100% threshold**

## Infrastructure (6 checks, max 12 pts)

| Check | Result | Score |
|---|---|---|
| INF-01 Non-root process | macOS app bundle — runs as user; no root required | 2 |
| INF-02 DB ports localhost only | N/A — no database | 2 |
| INF-03 Env vars for prod secrets | FAIL — server URL and port hardcoded in `constants.h` (no environment variable override) | 0 |
| INF-04 Health check endpoint | N/A — client application, no HTTP server | 2 |
| INF-05 Structured logging | Logger produces structured JSON to POST /v1/logs; `QLoggingCategory` throughout | 2 |
| INF-06 CI/CD config | FAIL — no CI/CD (removed per latest commit) | 0 |

**Infrastructure Score: 8/12 (67%)**

## Security Vulnerabilities — Snyk (5 checks, max 10 pts)

**SKIPPED** — Snyk CLI does not support C++/CMake projects.
Snyk reported: "Could not detect supported target files."

All 5 Snyk checks scored N/A (treated as 2 pts each to avoid unfair penalty for unsupported toolchain).

**Snyk Score: 10/10 (N/A — not applicable to C++ CMake project)**

## Scorecard Summary

| Category             | Score | Max | %    | Notes |
|---------------------|-------|-----|------|-------|
| Security             |  15   |  20 | 75%  | -5: no WSS; no rate limiting |
| Data Integrity       |  16   |  16 | 100% | N/A category (no database) |
| API Quality          |  15   |  16 | 94%  | Minor: HTTP log flush error handling |
| Code Quality         |   0   |  22 |  0%  | **BLOCKED** — CQ-09 (class docs), CQ-10 (method docs), CQ-11 (TODO found) |
| Test Quality         |  19   |  24 | 79%  | Coverage not measured at audit time |
| Infrastructure       |   8   |  12 | 67%  | -4: no env vars for URLs; no CI/CD |
| Snyk Vulnerabilities |  10   |  10 | N/A  | Snyk does not support C++/CMake |
| **OVERALL**          | **83**| **120** | **69%** | |

**Grade: C (69%)**

### Blocking Issues

1. **CQ-09 BLOCKING** — `PairionAudioPlayback` missing class-level Doxygen comment. Fix: add `/** @brief ... */` to `pairion_audio_playback.h` class declaration.
2. **CQ-10 BLOCKING** — `PairionAudioPlayback` public methods (`start`, `stop`, `handleOpusFrame`, `handlePcmFrame`, `handleStreamEnd`) missing doc comments.
3. **CQ-11 BLOCKING** — `TODO(PC-003)` at `audio_session_orchestrator.cpp:89` — pre-roll audio dropped (not Opus-encoded before sending). This is incomplete code.

### Notable Issues (Non-Blocking)

- Wake detector writes to `/tmp/pairion_wake_scores.csv` on every inference — production debug artifact
- Server URL/port hardcoded in `constants.h` (no env var override)
- `ws://` plaintext WebSocket — requires `wss://` for production
- No CI/CD pipeline
- `kHeartbeatDeadlineMs` constant defined but never enforced

