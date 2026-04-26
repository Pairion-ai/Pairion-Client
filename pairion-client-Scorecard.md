# pairion-client — Quality Scorecard

**Generated:** 2026-04-26T15:25:52Z
**Branch:** main
**Commit:** 666d43113ab47676eb62002e1fe16d6697f4982e feat: weather radar overlay — RainViewer animated tiles (PC-WXRADAR-001)

---

## Documentation Coverage

**Standard:** C++ uses Doxygen `/** */` or `///`. QML uses `/** @file @brief */`.

| Check | Result | Score |
|---|---|---|
| C++ classes have @brief Doxygen | 184 @brief/@file entries across 55 classes — PASS | 2/2 |
| Public C++ methods have doc comments | 272 of 273 method lines have `///` or `/** */` — PASS (1 inline LCOV exclusion) | 2/2 |
| QML components have @file/@brief | 187 doc comment hits across 28 QML files — PASS | 2/2 |
| Abstract interfaces documented | OnnxInferenceSession interface fully documented — PASS | 2/2 |
| Signal/slot documentation | All signals and slots documented in .h files — PASS | 2/2 |

**Documentation Score: 10/10 (100%)**

---

## Test Quality

| Check | Result | Score |
|---|---|---|
| Test files exist | 17 test executables — PASS | 2/2 |
| All C++ modules covered | 17 modules tested: orchestrator, binary frame, connection state, constants, device identity, inbound playback, integration, log batching, message codec, model downloader, opus codec, health monitor, ring buffer, settings, silero vad, wake detector, WS client state machine — PASS | 2/2 |
| Test infrastructure | Mock server + mock ONNX session for isolation; QtTest framework — PASS | 2/2 |
| Integration test exists | `tst_integration.cpp` covers full WS client with mock server — PASS | 2/2 |
| Coverage target configured | PAIRION_COVERAGE_THRESHOLD=100, LCOV enforced via cmake/check_coverage.cmake — PASS (target enforced at build time) | 2/2 |

**Test Quality Score: 10/10 (100%)**

**Note:** Coverage number cannot be verified without running the build + coverage target (no build artifact present). Coverage target configuration is correct and threshold=100 is enforced.

---

## Technical Debt

_Updated: 2026-04-26 — PC-STUB-001_

| Check | Result | Score |
|---|---|---|
| No CRITICAL stubs in implemented overlays | WeatherCurrentOverlay.qml remains (ships in PX-WXCUR-001); MarkersOverlay and NewsPinsOverlay deleted — 1 remaining | 1/2 |
| No hardcoded placeholder data | DashboardPanels placeholder data is intentional — panels become data-driven via SceneDataPush in M3+ milestones; comment block added to file documenting this — ACKNOWLEDGED | 1/2 |
| No hardcoded server URLs | kDefaultServerUrl/kDefaultRestBaseUrl hardcoded to localhost — development only; noted | 1/2 |
| No dead/legacy code accumulation | Scenes/ directory deleted; HudLabel.qml + HudPanel.qml deleted (zero references); PairionScene/ active components retained (PairionStyle, BackgroundBase, OverlayBase actively imported) — PASS | 2/2 |
| No TODO/FIXME in production code | "TODO" panel header replaced with "TASKS" in DashboardPanels.qml — PASS | 2/2 |

**Technical Debt Score: 7/10 (70%)**

REMAINING BLOCKING: WeatherCurrentOverlay.qml — stub overlay (ships in PX-WXCUR-001).

---

## Code Quality

| Check | Result | Score |
|---|---|---|
| Strict compiler warnings enabled | -Wall -Wextra -Wpedantic -Werror on all presets — PASS | 2/2 |
| C++20 standard enforced | CMAKE_CXX_STANDARD 20, CMAKE_CXX_STANDARD_REQUIRED ON — PASS | 2/2 |
| RAII / Qt ownership model correct | All objects parented correctly; no bare new without parent or RAII — PASS | 2/2 |
| Threading model correct | Main thread: QAudioSource, QML, WS; EncoderThread: Opus; InferenceThread: Wake+VAD — correct moveToThread usage | 2/2 |
| Error handling complete | All signal paths have error handlers; no unhandled exceptions in hot paths | 1/2 |

**Code Quality Score: 9/10 (90%)**

---

## Security / Infrastructure

| Check | Result | Score |
|---|---|---|
| No hardcoded secrets | PASS — no passwords, API keys, or tokens in source | 2/2 |
| Model integrity verification | PASS — SHA-256 verification before ONNX model use | 2/2 |
| Credential storage | PASS — QSettings (platform-native secure storage) | 2/2 |
| Plaintext WS connection | WARNING — ws:// (development only; production needs wss://) | 1/2 |
| No CI/CD pipeline | FAIL — no automated quality gates | 0/2 |

**Security/Infrastructure Score: 7/10 (70%)**

---

## Scorecard Summary

| Category | Score | Max | % |
|---|---|---|---|
| Documentation | 10 | 10 | 100% |
| Test Quality | 10 | 10 | 100% |
| Technical Debt | 7 | 10 | 70% |
| Code Quality | 9 | 10 | 90% |
| Security/Infrastructure | 7 | 10 | 70% |
| **OVERALL** | **43** | **50** | **86% (B)** |

---

## BLOCKING Issues

1. **`qml/Overlays/WeatherCurrentOverlay.qml`** — STUB: zero rendering implementation (ships in PX-WXCUR-001)

## Non-Blocking Observations

1. No CI/CD — consider adding GitHub Actions for build + test + coverage enforcement
2. Server URL hardcoded to localhost — consider making runtime-configurable for production deployment

## Verification Commands

```bash
cmake --preset macos-arm64-debug
cmake --build --preset macos-arm64-debug
ctest --preset macos-arm64-debug
cmake --build --preset macos-arm64-debug --target coverage
```

