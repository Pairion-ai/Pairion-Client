/**
 * @file tst_audio_session_orchestrator.cpp
 * @brief Tests for the AudioSessionOrchestrator state machine.
 */

#include "../src/core/constants.h"
#include "../src/pipeline/audio_session_orchestrator.h"
#include "../src/state/connection_state.h"
#include "mock_onnx_session.h"
#include "mock_server.h"

#include <QSignalSpy>
#include <QTest>

using namespace pairion::pipeline;
using namespace pairion::audio;
using namespace pairion::wake;
using namespace pairion::vad;
using namespace pairion::ws;
using namespace pairion::state;
using namespace pairion::test;

class TestAudioSessionOrchestrator : public QObject {
    Q_OBJECT

  private slots:
    /// Verify initial state is Idle.
    void initialStateIsIdle() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient wsClient(server.url(), QStringLiteral("d"), QStringLiteral("t"),
                                        &connState);

        PairionAudioCapture capture(static_cast<QIODevice *>(nullptr));
        PairionOpusEncoder encoder;
        PairionAudioPlayback playback;
        MockOnnxSession mel, emb, cls, vadSess;
        OpenWakewordDetector wake(&mel, &emb, &cls, 0.5);
        SileroVad vad(&vadSess, 0.5, 800);

        AudioSessionOrchestrator orch(&capture, &encoder, &wake, &vad, &wsClient, &connState,
                                      &playback);
        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::Idle);
    }

    /// Verify startListening transitions to AwaitingWake.
    void startListeningTransitions() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient wsClient(server.url(), QStringLiteral("d"), QStringLiteral("t"),
                                        &connState);

        PairionAudioCapture capture(static_cast<QIODevice *>(nullptr));
        PairionOpusEncoder encoder;
        MockOnnxSession mel, emb, cls, vadSess;
        OpenWakewordDetector wake(&mel, &emb, &cls, 0.5);
        SileroVad vad(&vadSess, 0.5, 800);

        AudioSessionOrchestrator orch(&capture, &encoder, &wake, &vad, &wsClient, &connState);
        orch.startListening();
        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::AwaitingWake);
        QCOMPARE(connState.voiceState(), QStringLiteral("awaiting_wake"));
    }

    /// Verify wake detection transitions to Streaming and sends protocol messages.
    void wakeTransitionsToStreaming() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient wsClient(server.url(), QStringLiteral("d"), QStringLiteral("t"),
                                        &connState);
        QSignalSpy sessionSpy(&wsClient, &PairionWebSocketClient::sessionOpened);
        wsClient.connectToServer();
        QVERIFY(sessionSpy.wait(5000));

        PairionAudioCapture capture(static_cast<QIODevice *>(nullptr));
        PairionOpusEncoder encoder;
        MockOnnxSession mel, emb, cls, vadSess;
        OpenWakewordDetector wake(&mel, &emb, &cls, 0.5);
        SileroVad vad(&vadSess, 0.5, 800);

        AudioSessionOrchestrator orch(&capture, &encoder, &wake, &vad, &wsClient, &connState);
        QSignalSpy wakeSpy(&orch, &AudioSessionOrchestrator::wakeFired);
        orch.startListening();

        // Simulate wake detection
        emit wake.wakeWordDetected(0.9f, QByteArray(640, '\0'));

        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::Streaming);
        QCOMPARE(wakeSpy.count(), 1);
        QVERIFY(!wakeSpy.at(0).at(0).toString().isEmpty()); // stream ID

        // Verify WakeWordDetected was sent to server
        QTest::qWait(200);
        bool foundWake = false;
        for (const auto &msg : server.receivedMessages()) {
            if (msg.contains(QLatin1String("WakeWordDetected"))) {
                foundWake = true;
                break;
            }
        }
        QVERIFY(foundWake);

        wsClient.disconnectFromServer();
    }

    /// Verify speech ended transitions back to Idle (then auto-restarts listening).
    void speechEndedTransitions() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient wsClient(server.url(), QStringLiteral("d"), QStringLiteral("t"),
                                        &connState);
        QSignalSpy sessionSpy(&wsClient, &PairionWebSocketClient::sessionOpened);
        wsClient.connectToServer();
        QVERIFY(sessionSpy.wait(5000));

        PairionAudioCapture capture(static_cast<QIODevice *>(nullptr));
        PairionOpusEncoder encoder;
        MockOnnxSession mel, emb, cls, vadSess;
        OpenWakewordDetector wake(&mel, &emb, &cls, 0.5);
        SileroVad vad(&vadSess, 0.5, 800);

        AudioSessionOrchestrator orch(&capture, &encoder, &wake, &vad, &wsClient, &connState);
        QSignalSpy endSpy(&orch, &AudioSessionOrchestrator::streamEnded);

        orch.startListening();
        emit wake.wakeWordDetected(0.9f, QByteArray(640, '\0'));
        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::Streaming);

        // Simulate speech ended
        emit vad.speechEnded();

        QCOMPARE(endSpy.count(), 1);
        // Auto-restarts to AwaitingWake
        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::AwaitingWake);

        wsClient.disconnectFromServer();
    }

    /// Verify shutdown stops the pipeline.
    void shutdownStops() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient wsClient(server.url(), QStringLiteral("d"), QStringLiteral("t"),
                                        &connState);

        PairionAudioCapture capture(static_cast<QIODevice *>(nullptr));
        PairionOpusEncoder encoder;
        MockOnnxSession mel, emb, cls, vadSess;
        OpenWakewordDetector wake(&mel, &emb, &cls, 0.5);
        SileroVad vad(&vadSess, 0.5, 800);

        AudioSessionOrchestrator orch(&capture, &encoder, &wake, &vad, &wsClient, &connState);
        orch.startListening();
        orch.shutdown();
        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::Idle);
    }

    /// Verify opus frame forwarding during streaming.
    void opusFrameForwarded() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient wsClient(server.url(), QStringLiteral("d"), QStringLiteral("t"),
                                        &connState);
        QSignalSpy sessionSpy(&wsClient, &PairionWebSocketClient::sessionOpened);
        wsClient.connectToServer();
        QVERIFY(sessionSpy.wait(5000));

        PairionAudioCapture capture(static_cast<QIODevice *>(nullptr));
        PairionOpusEncoder encoder;
        MockOnnxSession mel, emb, cls, vadSess;
        OpenWakewordDetector wake(&mel, &emb, &cls, 0.5);
        SileroVad vad(&vadSess, 0.5, 800);

        AudioSessionOrchestrator orch(&capture, &encoder, &wake, &vad, &wsClient, &connState);
        orch.startListening();
        emit wake.wakeWordDetected(0.9f, QByteArray(640, '\0'));

        // Simulate encoded opus frame
        emit encoder.opusFrameEncoded(QByteArray("opus_data"));
        QTest::qWait(200);

        QVERIFY(!server.receivedBinaryMessages().empty());

        wsClient.disconnectFromServer();
    }

    /// Verify startListening is no-op when not idle.
    void startListeningNoOpWhenNotIdle() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient wsClient(server.url(), QStringLiteral("d"), QStringLiteral("t"),
                                        &connState);

        PairionAudioCapture capture(static_cast<QIODevice *>(nullptr));
        PairionOpusEncoder encoder;
        MockOnnxSession mel, emb, cls, vadSess;
        OpenWakewordDetector wake(&mel, &emb, &cls, 0.5);
        SileroVad vad(&vadSess, 0.5, 800);

        AudioSessionOrchestrator orch(&capture, &encoder, &wake, &vad, &wsClient, &connState);
        orch.startListening();
        orch.startListening(); // no-op
        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::AwaitingWake);
    }

    /// Verify wakeWordDetected is a no-op when not in AwaitingWake state.
    void wakeWordGuardWhenNotAwaitingWake() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient wsClient(server.url(), QStringLiteral("d"), QStringLiteral("t"),
                                        &connState);

        PairionAudioCapture capture(static_cast<QIODevice *>(nullptr));
        PairionOpusEncoder encoder;
        PairionAudioPlayback playback;
        MockOnnxSession mel, emb, cls, vadSess;
        OpenWakewordDetector wake(&mel, &emb, &cls, 0.5);
        SileroVad vad(&vadSess, 0.5, 800);

        // Idle state — wakeWordDetected must not transition.
        AudioSessionOrchestrator orch(&capture, &encoder, &wake, &vad, &wsClient, &connState,
                                      &playback);
        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::Idle);
        emit wake.wakeWordDetected(0.9f, QByteArray(640, '\0'));
        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::Idle);
    }

    /// Verify speechEnded is a no-op when not in Streaming state.
    void speechEndedGuardWhenNotStreaming() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient wsClient(server.url(), QStringLiteral("d"), QStringLiteral("t"),
                                        &connState);

        PairionAudioCapture capture(static_cast<QIODevice *>(nullptr));
        PairionOpusEncoder encoder;
        MockOnnxSession mel, emb, cls, vadSess;
        OpenWakewordDetector wake(&mel, &emb, &cls, 0.5);
        SileroVad vad(&vadSess, 0.5, 800);

        AudioSessionOrchestrator orch(&capture, &encoder, &wake, &vad, &wsClient, &connState);
        orch.startListening(); // AwaitingWake
        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::AwaitingWake);
        // Emit speechEnded when not Streaming — must be ignored.
        emit vad.speechEnded();
        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::AwaitingWake);
    }

    /// Verify opusFrameEncoded is a no-op when not in Streaming state.
    void opusFrameGuardWhenNotStreaming() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient wsClient(server.url(), QStringLiteral("d"), QStringLiteral("t"),
                                        &connState);

        PairionAudioCapture capture(static_cast<QIODevice *>(nullptr));
        PairionOpusEncoder encoder;
        MockOnnxSession mel, emb, cls, vadSess;
        OpenWakewordDetector wake(&mel, &emb, &cls, 0.5);
        SileroVad vad(&vadSess, 0.5, 800);

        AudioSessionOrchestrator orch(&capture, &encoder, &wake, &vad, &wsClient, &connState);
        orch.startListening();                              // AwaitingWake — not Streaming
        emit encoder.opusFrameEncoded(QByteArray("frame")); // must be ignored
        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::AwaitingWake);
    }

    /// Verify shutdown() when already Idle is a no-op (covers transitionTo same-state guard).
    void shutdownWhenAlreadyIdleIsNoOp() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient wsClient(server.url(), QStringLiteral("d"), QStringLiteral("t"),
                                        &connState);

        PairionAudioCapture capture(static_cast<QIODevice *>(nullptr));
        PairionOpusEncoder encoder;
        MockOnnxSession mel, emb, cls, vadSess;
        OpenWakewordDetector wake(&mel, &emb, &cls, 0.5);
        SileroVad vad(&vadSess, 0.5, 800);

        AudioSessionOrchestrator orch(&capture, &encoder, &wake, &vad, &wsClient, &connState);
        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::Idle);
        orch.shutdown(); // already Idle — transitionTo(Idle) hits same-state early return
        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::Idle);
    }

    // ── Barge-in tests ──────────────────────────────────────────────────────

    /// Verify TTS starting while streaming transitions orchestrator to PlayingBack.
    void ttsStartTransitionsToPlayingBack() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient wsClient(server.url(), QStringLiteral("d"), QStringLiteral("t"),
                                        &connState);
        QSignalSpy sessionSpy(&wsClient, &PairionWebSocketClient::sessionOpened);
        wsClient.connectToServer();
        QVERIFY(sessionSpy.wait(5000));

        PairionAudioCapture capture(static_cast<QIODevice *>(nullptr));
        PairionOpusEncoder encoder;
        PairionAudioPlayback playback;
        MockOnnxSession mel, emb, cls, vadSess;
        OpenWakewordDetector wake(&mel, &emb, &cls, 0.5);
        SileroVad vad(&vadSess, 0.5, 800);

        AudioSessionOrchestrator orch(&capture, &encoder, &wake, &vad, &wsClient, &connState,
                                      &playback);
        orch.startListening();
        emit wake.wakeWordDetected(0.9f, QByteArray(640, '\0'));
        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::Streaming);

        // Simulate TTS start
        emit playback.speakingStateChanged(QStringLiteral("speaking"));

        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::PlayingBack);
        QCOMPARE(connState.voiceState(), QStringLiteral("playing_back"));

        wsClient.disconnectFromServer();
    }

    /// Verify TTS starting while awaiting wake also transitions to PlayingBack.
    void ttsStartWhileAwaitingWakeTransitionsToPlayingBack() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient wsClient(server.url(), QStringLiteral("d"), QStringLiteral("t"),
                                        &connState);

        PairionAudioCapture capture(static_cast<QIODevice *>(nullptr));
        PairionOpusEncoder encoder;
        PairionAudioPlayback playback;
        MockOnnxSession mel, emb, cls, vadSess;
        OpenWakewordDetector wake(&mel, &emb, &cls, 0.5);
        SileroVad vad(&vadSess, 0.5, 800);

        AudioSessionOrchestrator orch(&capture, &encoder, &wake, &vad, &wsClient, &connState,
                                      &playback);
        orch.startListening();
        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::AwaitingWake);

        emit playback.speakingStateChanged(QStringLiteral("speaking"));

        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::PlayingBack);
    }

    /// Verify TTS start while Idle is ignored (no state change).
    void ttsStartWhileIdleIgnored() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient wsClient(server.url(), QStringLiteral("d"), QStringLiteral("t"),
                                        &connState);

        PairionAudioCapture capture(static_cast<QIODevice *>(nullptr));
        PairionOpusEncoder encoder;
        PairionAudioPlayback playback;
        MockOnnxSession mel, emb, cls, vadSess;
        OpenWakewordDetector wake(&mel, &emb, &cls, 0.5);
        SileroVad vad(&vadSess, 0.5, 800);

        AudioSessionOrchestrator orch(&capture, &encoder, &wake, &vad, &wsClient, &connState,
                                      &playback);
        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::Idle);

        // TTS fires while Idle — must be ignored
        emit playback.speakingStateChanged(QStringLiteral("speaking"));

        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::Idle);
    }

    /// Verify TTS second burst during PlayingBack resets barge-in timer without re-entering.
    void ttsRestartDuringPlayingBackResetsTimer() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient wsClient(server.url(), QStringLiteral("d"), QStringLiteral("t"),
                                        &connState);

        PairionAudioCapture capture(static_cast<QIODevice *>(nullptr));
        PairionOpusEncoder encoder;
        PairionAudioPlayback playback;
        MockOnnxSession mel, emb, cls, vadSess;
        OpenWakewordDetector wake(&mel, &emb, &cls, 0.5);
        SileroVad vad(&vadSess, 0.5, 800);

        AudioSessionOrchestrator orch(&capture, &encoder, &wake, &vad, &wsClient, &connState,
                                      &playback);
        orch.startListening();
        emit playback.speakingStateChanged(QStringLiteral("speaking")); // → PlayingBack
        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::PlayingBack);

        // Second TTS burst — must stay in PlayingBack, not crash
        emit playback.speakingStateChanged(QStringLiteral("speaking"));
        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::PlayingBack);
    }

    /// Verify TTS ending naturally returns to AwaitingWake.
    void ttsEndNaturallyResumesListening() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient wsClient(server.url(), QStringLiteral("d"), QStringLiteral("t"),
                                        &connState);

        PairionAudioCapture capture(static_cast<QIODevice *>(nullptr));
        PairionOpusEncoder encoder;
        PairionAudioPlayback playback;
        MockOnnxSession mel, emb, cls, vadSess;
        OpenWakewordDetector wake(&mel, &emb, &cls, 0.5);
        SileroVad vad(&vadSess, 0.5, 800);

        AudioSessionOrchestrator orch(&capture, &encoder, &wake, &vad, &wsClient, &connState,
                                      &playback);
        orch.startListening();
        emit playback.speakingStateChanged(QStringLiteral("speaking")); // → PlayingBack
        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::PlayingBack);

        emit playback.speakingStateChanged(QStringLiteral("idle")); // → AwaitingWake

        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::AwaitingWake);
        QCOMPARE(connState.voiceState(), QStringLiteral("awaiting_wake"));
    }

    /// Verify TTS end while NOT in PlayingBack is ignored.
    void ttsEndIgnoredWhenNotPlayingBack() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient wsClient(server.url(), QStringLiteral("d"), QStringLiteral("t"),
                                        &connState);

        PairionAudioCapture capture(static_cast<QIODevice *>(nullptr));
        PairionOpusEncoder encoder;
        PairionAudioPlayback playback;
        MockOnnxSession mel, emb, cls, vadSess;
        OpenWakewordDetector wake(&mel, &emb, &cls, 0.5);
        SileroVad vad(&vadSess, 0.5, 800);

        AudioSessionOrchestrator orch(&capture, &encoder, &wake, &vad, &wsClient, &connState,
                                      &playback);
        orch.startListening();
        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::AwaitingWake);

        emit playback.speakingStateChanged(QStringLiteral("idle")); // not in PlayingBack — ignore

        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::AwaitingWake);
    }

    /// Verify VAD threshold is raised when entering PlayingBack.
    void vadThresholdRaisedDuringPlayback() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient wsClient(server.url(), QStringLiteral("d"), QStringLiteral("t"),
                                        &connState);

        PairionAudioCapture capture(static_cast<QIODevice *>(nullptr));
        PairionOpusEncoder encoder;
        PairionAudioPlayback playback;
        MockOnnxSession mel, emb, cls, vadSess;
        OpenWakewordDetector wake(&mel, &emb, &cls, 0.5);
        SileroVad vad(&vadSess, 0.3, 800); // normal threshold = 0.3

        AudioSessionOrchestrator orch(&capture, &encoder, &wake, &vad, &wsClient, &connState,
                                      &playback);
        orch.startListening();
        emit playback.speakingStateChanged(QStringLiteral("speaking")); // → PlayingBack

        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::PlayingBack);
        QCOMPARE(vad.threshold(), pairion::kBargeInVadThreshold);
    }

    /// Verify VAD threshold is restored when PlayingBack ends naturally.
    void vadThresholdRestoredAfterPlayback() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient wsClient(server.url(), QStringLiteral("d"), QStringLiteral("t"),
                                        &connState);

        PairionAudioCapture capture(static_cast<QIODevice *>(nullptr));
        PairionOpusEncoder encoder;
        PairionAudioPlayback playback;
        MockOnnxSession mel, emb, cls, vadSess;
        OpenWakewordDetector wake(&mel, &emb, &cls, 0.5);
        SileroVad vad(&vadSess, 0.3, 800); // normal threshold = 0.3

        AudioSessionOrchestrator orch(&capture, &encoder, &wake, &vad, &wsClient, &connState,
                                      &playback);
        orch.startListening();
        emit playback.speakingStateChanged(QStringLiteral("speaking")); // → PlayingBack
        emit playback.speakingStateChanged(QStringLiteral("idle"));     // → AwaitingWake

        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::AwaitingWake);
        QCOMPARE(vad.threshold(), 0.3);
    }

    /// Verify speechStarted during PlayingBack starts the barge-in timer.
    void speechStartedDuringPlaybackStartsBargeInTimer() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient wsClient(server.url(), QStringLiteral("d"), QStringLiteral("t"),
                                        &connState);

        PairionAudioCapture capture(static_cast<QIODevice *>(nullptr));
        PairionOpusEncoder encoder;
        PairionAudioPlayback playback;
        MockOnnxSession mel, emb, cls, vadSess;
        OpenWakewordDetector wake(&mel, &emb, &cls, 0.5);
        SileroVad vad(&vadSess, 0.5, 800);

        AudioSessionOrchestrator orch(&capture, &encoder, &wake, &vad, &wsClient, &connState,
                                      &playback);
        orch.setBargeInTimerIntervalMs(300); // keep test fast
        orch.startListening();
        emit playback.speakingStateChanged(QStringLiteral("speaking")); // → PlayingBack

        // Emit speechStarted — barge-in timer must start
        emit vad.speechStarted();

        // VAD reports speech ended before timer expires → under-breath filter
        emit vad.speechEnded();

        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::PlayingBack);
    }

    /// Verify speechStarted during AwaitingWake does NOT start the barge-in timer.
    void speechStartedDuringAwaitingWakeIgnored() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient wsClient(server.url(), QStringLiteral("d"), QStringLiteral("t"),
                                        &connState);

        PairionAudioCapture capture(static_cast<QIODevice *>(nullptr));
        PairionOpusEncoder encoder;
        PairionAudioPlayback playback;
        MockOnnxSession mel, emb, cls, vadSess;
        OpenWakewordDetector wake(&mel, &emb, &cls, 0.5);
        SileroVad vad(&vadSess, 0.5, 800);

        AudioSessionOrchestrator orch(&capture, &encoder, &wake, &vad, &wsClient, &connState,
                                      &playback);
        orch.startListening(); // → AwaitingWake

        // speechStarted during AwaitingWake must be ignored
        emit vad.speechStarted();
        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::AwaitingWake);
    }

    /// Verify speechEnded during PlayingBack with no active timer is a no-op.
    void speechEndedDuringPlaybackNoTimerIsNoOp() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient wsClient(server.url(), QStringLiteral("d"), QStringLiteral("t"),
                                        &connState);

        PairionAudioCapture capture(static_cast<QIODevice *>(nullptr));
        PairionOpusEncoder encoder;
        PairionAudioPlayback playback;
        MockOnnxSession mel, emb, cls, vadSess;
        OpenWakewordDetector wake(&mel, &emb, &cls, 0.5);
        SileroVad vad(&vadSess, 0.5, 800);

        AudioSessionOrchestrator orch(&capture, &encoder, &wake, &vad, &wsClient, &connState,
                                      &playback);
        orch.startListening();
        emit playback.speakingStateChanged(QStringLiteral("speaking")); // → PlayingBack

        // No speechStarted was emitted so barge-in timer is not active
        emit vad.speechEnded(); // must be no-op

        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::PlayingBack);
    }

    /// Verify confirmed barge-in (timer expiry) sends BargeIn and transitions to Streaming.
    void bargeInTimerExpiryTransitionsToStreaming() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient wsClient(server.url(), QStringLiteral("d"), QStringLiteral("t"),
                                        &connState);
        QSignalSpy sessionSpy(&wsClient, &PairionWebSocketClient::sessionOpened);
        wsClient.connectToServer();
        QVERIFY(sessionSpy.wait(5000));

        PairionAudioCapture capture(static_cast<QIODevice *>(nullptr));
        PairionOpusEncoder encoder;
        PairionAudioPlayback playback;
        MockOnnxSession mel, emb, cls, vadSess;
        OpenWakewordDetector wake(&mel, &emb, &cls, 0.5);
        SileroVad vad(&vadSess, 0.5, 800);

        AudioSessionOrchestrator orch(&capture, &encoder, &wake, &vad, &wsClient, &connState,
                                      &playback);
        orch.setBargeInTimerIntervalMs(1); // fire immediately
        QSignalSpy wakeSpy(&orch, &AudioSessionOrchestrator::wakeFired);

        // Seed pre-roll buffer with 2 frames (1280 bytes) so executeBargeIn()
        // exercises the encoding loop (lines that are otherwise unreachable with
        // an empty wake detector buffer in tests).
        wake.setPreRollBufferForTest(QByteArray(1280, '\0'));

        orch.startListening();
        emit playback.speakingStateChanged(QStringLiteral("speaking")); // → PlayingBack
        emit vad.speechStarted(); // start barge-in timer (1 ms)

        // Wait for the barge-in timer to expire and executeBargeIn() to run
        QVERIFY(QTest::qWaitFor([&]() {
            return orch.state() == AudioSessionOrchestrator::State::Streaming;
        }, 2000));

        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::Streaming);
        // wakeFired must have been emitted for the new barge-in stream
        QCOMPARE(wakeSpy.count(), 1);

        // Verify BargeIn message was sent to server
        QTest::qWait(200);
        bool foundBargeIn = false;
        for (const auto &msg : server.receivedMessages()) {
            if (msg.contains(QLatin1String("BargeIn"))) {
                foundBargeIn = true;
                break;
            }
        }
        QVERIFY(foundBargeIn);

        // Verify AudioStreamStart was sent for the new stream
        bool foundAudioStart = false;
        for (const auto &msg : server.receivedMessages()) {
            if (msg.contains(QLatin1String("AudioStreamStart"))) {
                foundAudioStart = true;
                break;
            }
        }
        QVERIFY(foundAudioStart);

        wsClient.disconnectFromServer();
    }

    /// Verify barge-in timer expiry while NOT in PlayingBack is a no-op.
    void bargeInTimerGuardWhenNotPlayingBack() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient wsClient(server.url(), QStringLiteral("d"), QStringLiteral("t"),
                                        &connState);

        PairionAudioCapture capture(static_cast<QIODevice *>(nullptr));
        PairionOpusEncoder encoder;
        PairionAudioPlayback playback;
        MockOnnxSession mel, emb, cls, vadSess;
        OpenWakewordDetector wake(&mel, &emb, &cls, 0.5);
        SileroVad vad(&vadSess, 0.5, 800);

        AudioSessionOrchestrator orch(&capture, &encoder, &wake, &vad, &wsClient, &connState,
                                      &playback);
        orch.startListening(); // → AwaitingWake (not PlayingBack)

        // Manually invoke the timer slot — must be a no-op since not in PlayingBack
        QMetaObject::invokeMethod(&orch, "onBargeInTimerExpired");
        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::AwaitingWake);
    }

    /// Verify ttsStartTransitionsToPlayingBack also emits streamEnded when in Streaming.
    void ttsStartEmitsStreamEndedWhenStreaming() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient wsClient(server.url(), QStringLiteral("d"), QStringLiteral("t"),
                                        &connState);
        QSignalSpy sessionSpy(&wsClient, &PairionWebSocketClient::sessionOpened);
        wsClient.connectToServer();
        QVERIFY(sessionSpy.wait(5000));

        PairionAudioCapture capture(static_cast<QIODevice *>(nullptr));
        PairionOpusEncoder encoder;
        PairionAudioPlayback playback;
        MockOnnxSession mel, emb, cls, vadSess;
        OpenWakewordDetector wake(&mel, &emb, &cls, 0.5);
        SileroVad vad(&vadSess, 0.5, 800);

        AudioSessionOrchestrator orch(&capture, &encoder, &wake, &vad, &wsClient, &connState,
                                      &playback);
        QSignalSpy endSpy(&orch, &AudioSessionOrchestrator::streamEnded);

        orch.startListening();
        emit wake.wakeWordDetected(0.9f, QByteArray(640, '\0'));
        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::Streaming);

        emit playback.speakingStateChanged(QStringLiteral("speaking")); // → PlayingBack

        QCOMPARE(endSpy.count(), 1);
        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::PlayingBack);

        wsClient.disconnectFromServer();
    }

    /// Verify inbound audio stream ID is stored and used in BargeIn message.
    void bargeInUsesInboundStreamId() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient wsClient(server.url(), QStringLiteral("d"), QStringLiteral("t"),
                                        &connState);
        QSignalSpy sessionSpy(&wsClient, &PairionWebSocketClient::sessionOpened);
        wsClient.connectToServer();
        QVERIFY(sessionSpy.wait(5000));

        PairionAudioCapture capture(static_cast<QIODevice *>(nullptr));
        PairionOpusEncoder encoder;
        PairionAudioPlayback playback;
        MockOnnxSession mel, emb, cls, vadSess;
        OpenWakewordDetector wake(&mel, &emb, &cls, 0.5);
        SileroVad vad(&vadSess, 0.5, 800);

        AudioSessionOrchestrator orch(&capture, &encoder, &wake, &vad, &wsClient, &connState,
                                      &playback);
        orch.setBargeInTimerIntervalMs(1);
        orch.startListening();

        // Simulate server sending an AudioStreamStart for its TTS stream
        emit wsClient.audioStreamStartOutReceived(QStringLiteral("server-stream-99"));

        emit playback.speakingStateChanged(QStringLiteral("speaking")); // → PlayingBack
        emit vad.speechStarted();

        QVERIFY(QTest::qWaitFor([&]() {
            return orch.state() == AudioSessionOrchestrator::State::Streaming;
        }, 2000));

        // Find BargeIn message and verify it references the correct inbound stream ID
        QTest::qWait(100);
        bool foundBargeInWithId = false;
        for (const auto &msg : server.receivedMessages()) {
            if (msg.contains(QLatin1String("BargeIn"))
                && msg.contains(QLatin1String("server-stream-99"))) {
                foundBargeInWithId = true;
                break;
            }
        }
        QVERIFY(foundBargeInWithId);

        wsClient.disconnectFromServer();
    }

    /// Verify pre-roll PCM is Opus-encoded and sent as binary frames before live streaming.
    void preRollEncodedAndSentAsBinaryFrames() {
        MockServer server;
        ConnectionState connState;
        PairionWebSocketClient wsClient(server.url(), QStringLiteral("d"), QStringLiteral("t"),
                                        &connState);
        QSignalSpy sessionSpy(&wsClient, &PairionWebSocketClient::sessionOpened);
        wsClient.connectToServer();
        QVERIFY(sessionSpy.wait(5000));

        PairionAudioCapture capture(static_cast<QIODevice *>(nullptr));
        PairionOpusEncoder encoder;
        MockOnnxSession mel, emb, cls, vadSess;
        OpenWakewordDetector wake(&mel, &emb, &cls, 0.5);
        SileroVad vad(&vadSess, 0.5, 800);

        AudioSessionOrchestrator orch(&capture, &encoder, &wake, &vad, &wsClient, &connState);
        orch.startListening();

        // Provide exactly 2 PCM frames worth of pre-roll (2 × 640 bytes = 1280 bytes).
        // m_preRollEncoder encodes each 640-byte chunk into a compressed Opus frame.
        QByteArray preRoll(1280, '\0');
        emit wake.wakeWordDetected(0.9f, preRoll);

        // Allow the WS send queue to flush.
        QTest::qWait(200);

        // At least 2 binary frames must have arrived: one per 640-byte PCM chunk.
        QVERIFY(server.receivedBinaryMessages().size() >= 2);

        // Each frame = 4-byte stream-ID prefix + Opus payload.
        // Opus payload must be smaller than raw PCM (640 bytes), proving encoding occurred.
        for (const auto &frame : server.receivedBinaryMessages()) {
            QVERIFY(frame.size() > 4);       // non-empty beyond prefix
            QVERIFY(frame.size() < 4 + 640); // Opus-compressed, not raw PCM
        }

        wsClient.disconnectFromServer();
    }
};

QTEST_GUILESS_MAIN(TestAudioSessionOrchestrator)
#include "tst_audio_session_orchestrator.moc"
