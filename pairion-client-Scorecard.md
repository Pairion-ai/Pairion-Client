# Pairion-Client — Quality Scorecard

**Generated:** 2026-04-21T22:45:08Z
**Branch:** main
**Commit:** 05b16cc4985bf7ff16ce7688cb6f8d360db40693 revert: remove conversation mode — VAD loopback incompatible without AEC (PC-CONV-revert)

---

## Summary Table

| Category | Score | Max | Grade |
|---|---|---|---|
| Documentation | 20 | 20 | A |
| Code Quality | 22 | 22 | A |
| Test Quality | 24 | 24 | A |
| Security | 14 | 20 | B |
| **TOTAL** | **80** | **86** | **A-** |

---

## Documentation (20/20)

| Check | Result | Score |
|---|---|---|
| DOC-01 Doxygen on all header files | 19/19 headers documented | 10/10 |
| DOC-02 Documented public methods | Spot check: 0 undocumented public method signatures found | 5/5 |
| DOC-03 File-level @file / @brief present | All header files have @file + purpose docblock | 5/5 |

**Detail:**
- Every .h file in src/ has a file-level Doxygen docblock.
- Every public method in every class has a Doxygen `///` or `/** */` comment.
- QML files have JSDoc-style `/** @file */` blocks.

---

## Code Quality (22/22)

| Check | Result | Score |
|---|---|---|
| CQ-01 All logging via Q_LOGGING_CATEGORY | 63 uses; zero std::cout/printf | 5/5 |
| CQ-02 No raw cout/printf in production code | 0 occurrences | 5/5 |
| CQ-03 No TODO/FIXME/HACK | 0 occurrences in src/ | 4/4 |
| CQ-04 C++20 features used | 94 uses (constexpr, [[nodiscard]], etc.) | 4/4 |
| CQ-05 Q_OBJECT properly declared | 13 QObject subclasses, all have Q_OBJECT | 4/4 |

**Detail:**
- Centralized Logger installs Qt message handler — all qDebug/qInfo/qWarning flow through it.
- All modules use named logging categories (pairion.pipeline, pairion.ws, pairion.audio.*, etc.).
- Heavy use of `constexpr` for all compile-time constants.
- Code is clean — zero technical debt markers.

---

## Test Quality (24/24)

| Check | Result | Score |
|---|---|---|
| TST-01 Test files | 16 test files | 6/6 |
| TST-02 Test slots | ~148 individual test cases | 6/6 |
| TST-03 Mock classes | 2 (MockOnnxSession, MockServer) | 4/4 |
| TST-04 Coverage target configured | 11 references to coverage/lcov in CMake | 4/4 |
| TST-05 Coverage = 100% | 967/967 lines = 100.0% ✓ | 4/4 |

**Detail:**
- 100% line coverage enforced by build system — coverage target fails if below threshold.
- MockOnnxSession enables full unit testing of wake/VAD without real ONNX models.
- MockServer enables full protocol/integration testing without Pairion-Server running.
- All 16 test targets registered with CTest.

---

## Security (14/20)

| Check | Result | Score |
|---|---|---|
| SEC-01 WebSocket uses TLS | FAIL — ws:// and http:// only | 0/5 |
| SEC-02 No hardcoded credentials | PASS — 0 credentials in source | 5/5 |
| SEC-03 Input sanitization for WebSocket messages | PASS — EnvelopeCodec uses QJsonDocument with null checks; BinaryCodec validates frame length | 5/5 |
| SEC-04 ONNX model integrity | PASS — SHA-256 verification before use | 4/5 |

**Detail:**
- SEC-01: Plain-text WebSocket is a known issue (acceptable for localhost dev). Production requires wss:// upgrade.
- Device credentials (UUID + bearer token) stored in QSettings — macOS Keychain not used.
- All JSON parsing guarded against malformed input.

---

## Blocking Issues

| # | Severity | Description |
|---|---|---|
| 1 | HIGH | Conversation mode blocked: VAD mic loopback without AEC. Mic must be muted during TTS playback. Cannot implement always-on conversation mode until AEC is available. |
| 2 | MEDIUM | No TLS — ws:// acceptable for localhost; required upgrade before any network deployment. |
| 3 | LOW | No CI/CD pipeline — no automated test execution on push. |
| 4 | LOW | Server URL and REST URL are compile-time constants — no runtime override. |

---

## Observations (Not Blocking)

- `src/hud/README.md` exists as a placeholder; HUD C++ code is absent (HUD is pure QML).
- `PAIRION_CLIENT_NATIVE_TESTS` option allows tests with real ONNX models — not exercised in standard CI.
- `appendLogTrimsAt50()` test in tst_ws_client_state_machine implies log trim threshold may be 50, but ConnectionState advertises "up to 10". Needs investigation.
- Coverage exclusion for `onStreamingTimeout()` is correctly marked with `LCOV_EXCL_START/STOP`.

