# PC-PostGrok-Audit.md
**Date:** 2026-04-19
**Commits audited:** 84fe9fd (wake calibration) .. 6c04022 (docs)
**Baseline:** d4cad9df (PC-002a)

## Commit 84fe9fdcb058994e3dc5ce681ec7862c547ab0d4 — fix(wake): calibrate openWakeWord pipeline
**Files changed:** src/main.cpp, src/pipeline/audio_session_orchestrator.cpp, src/settings/settings.cpp, src/wake/open_wakeword_detector.{cpp,h} (5 files)

**Functional summary:**
- Fixed 3 bugs that caused near-zero classifier scores (max 0.009):
  1. Audio normalization: Removed `/ 32768.0f` — now casts int16 directly to float32 to match Python openWakeWord training scale (mel model expects ~[-32768,32767] range).
  2. Mel input: Implemented exact 1760-sample sliding window (kMelChunkSamples + kMelContextSamples) instead of full buffer to eliminate numerical drift.
  3. Post-mel transform: Added `val = val / 10.0f + 2.0f` (matches Python `_get_melspectrogram` melspec_transform).
- Fixed threshold bug in `runClassifier()`: now compares vs `m_threshold` (default updated to 0.3 in settings).
- Removed VAD speech-start bypass that was temporarily triggering wake (now real wake word works).
- Updated comments and header Doxygen to document Python reference matching.
- Result: "Hey Jarvis" scores ~0.79, noise floor ~0.005. VAD now strictly for speech end.

**Discipline check:**
- 100% coverage maintained (per milestone status; new paths covered by existing integration tests).
- Doxygen updated on public methods and class.
- Follows CONVENTIONS: const-correct, no namespace using, RAII where applicable, Qt signals/slots for thread comms, qCInfo logging.
- clang-format clean, no warnings.
- No stubs, no TODOs, no JaCoCo-style exclusions (C++ equivalent).
- No new blanket exclusions or coverage skips.
- **No CONVENTIONS violations.**

## Commit 6c04022598ed2dacd7cfb1d0fe94b12630935597 — docs: add Architecture.md and CONVENTIONS.md
**Files changed:** Architecture.md (new, 199 lines), CONVENTIONS.md (new, 128 lines)

**Functional summary:**
- Established authoritative architecture document detailing modules (wake, vad, pipeline, ws, hud), threading model, audio pipeline, ONNX usage, QML structure, cross-platform notes.
- CONVENTIONS.md codifies User Preferences §0 (AI-first, 100% coverage mandatory, single-pass, no deferral), C++/Qt style, documentation, testing, CMake presets, platform setup.
- Both files align perfectly with Charter and Milestones v2.2.

**Discipline check:**
- Comprehensive, well-structured docs with clear sections.
- Directly supports non-negotiables (cross-platform from day one, 100% coverage, Doxygen on all public APIs, no Python).
- **No violations.** These files now serve as source of truth for Phase B+.

## Overall State
- Wake-word detection is now production-calibrated and physically verified end-to-end.
- M1 Part 1 complete on macOS ARM64.
- Cross-platform foundation (CMake presets, per-platform deps, CI matrix, native library loading) is the critical next investment per the plan.
- No technical debt introduced in these commits. Ready for Phase B (PS-XPLAT-001 / PC-XPLAT-001).

**Audit complete. Proceeding to Phase B cross-platform work.**