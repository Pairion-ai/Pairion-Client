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
        MockOnnxSession mel, emb, cls, vadSess;
        OpenWakewordDetector wake(&mel, &emb, &cls, 0.5);
        SileroVad vad(&vadSess, 0.5, 800);

        AudioSessionOrchestrator orch(&capture, &encoder, &wake, &vad, &wsClient, &connState);
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
};

QTEST_GUILESS_MAIN(TestAudioSessionOrchestrator)
#include "tst_audio_session_orchestrator.moc"
