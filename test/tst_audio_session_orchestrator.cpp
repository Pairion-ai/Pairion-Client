/**
 * @file tst_audio_session_orchestrator.cpp
 * @brief Tests for the AudioSessionOrchestrator state machine.
 */

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

    /// Verify wake then speech-ended enters ConversationWaiting (conversation mode on by default).
    void speechEndedEntersConversationWaiting() {
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
        QVERIFY(connState.conversationActive());

        // Simulate speech ended — should enter ConversationWaiting, not AwaitingWake
        emit vad.speechEnded();

        QCOMPARE(endSpy.count(), 1);
        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::ConversationWaiting);
        QCOMPARE(connState.voiceState(), QStringLiteral("conversation_waiting"));

        wsClient.disconnectFromServer();
    }

    /// Verify speechStarted in ConversationWaiting starts a new stream without wake word.
    void speechStartedInConversationWaitingStartsNewStream() {
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
        emit wake.wakeWordDetected(0.9f, QByteArray(640, '\0'));
        emit vad.speechEnded(); // → ConversationWaiting
        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::ConversationWaiting);

        // Simulate VAD detecting speech again — should start new stream
        emit vad.speechStarted();

        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::Streaming);
        QCOMPARE(wakeSpy.count(), 2); // one from wake word, one from conversation re-trigger

        wsClient.disconnectFromServer();
    }

    /// Verify conversationEnded signal exits conversation mode and returns to AwaitingWake.
    void conversationEndedExitsConversationMode() {
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
        emit vad.speechEnded(); // → ConversationWaiting
        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::ConversationWaiting);

        // Server signals conversation is over
        emit wsClient.conversationEndedReceived();

        QVERIFY(!connState.conversationActive());
        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::AwaitingWake);

        wsClient.disconnectFromServer();
    }

    /// Verify speechStarted is a no-op when not in ConversationWaiting state.
    void speechStartedGuardWhenNotConversationWaiting() {
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
        orch.startListening(); // AwaitingWake — not ConversationWaiting
        emit vad.speechStarted(); // must be ignored
        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::AwaitingWake);
    }

    /// Verify conversationEnded is a no-op when not in ConversationWaiting state.
    void conversationEndedGuardWhenNotConversationWaiting() {
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
        // conversationEnded while AwaitingWake — must not crash or double-transition
        emit wsClient.conversationEndedReceived();
        QCOMPARE(orch.state(), AudioSessionOrchestrator::State::AwaitingWake);
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
