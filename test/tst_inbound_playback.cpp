/**
 * @file tst_inbound_playback.cpp
 * @brief Tests for the inbound server audio pipeline:
 *        AudioStreamStart → binary Opus frames → AudioStreamEnd.
 *
 * Covers PairionAudioPlayback (preparePlayback, handleOpusFrame, handlePcmFrame,
 * handleStreamEnd, start, stop) and the orchestrator's inbound signal routing.
 */

#include "../src/audio/pairion_audio_capture.h"
#include "../src/audio/pairion_audio_playback.h"
#include "../src/audio/pairion_opus_encoder.h"
#include "../src/pipeline/audio_session_orchestrator.h"
#include "../src/protocol/binary_codec.h"
#include "../src/state/connection_state.h"
#include "mock_onnx_session.h"
#include "mock_server.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

using namespace pairion::audio;
using namespace pairion::pipeline;
using namespace pairion::wake;
using namespace pairion::vad;
using namespace pairion::ws;
using namespace pairion::state;
using namespace pairion::test;

/// Encode one 640-byte silence PCM frame to Opus on the calling thread.
static QByteArray encodeSilenceFrame() {
    QByteArray opusFrame;
    PairionOpusEncoder enc;
    QObject::connect(&enc, &PairionOpusEncoder::opusFrameEncoded,
                     [&opusFrame](const QByteArray &f) { opusFrame = f; });
    enc.encodePcmFrame(QByteArray(640, '\0'));
    return opusFrame;
}

class TestInboundPlayback : public QObject {
    Q_OBJECT

  private slots:
    /// Verify agentState transitions to "speaking" when inbound Opus binary frames arrive.
    void inboundAudioSetsAgentStateSpeaking() {
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

        const QString streamId = QStringLiteral("aaaabbbb-cccc-dddd-eeee-ffffffffffff");

        // Server sends AudioStreamStart (triggers preparePlayback).
        QJsonObject startMsg;
        startMsg[QStringLiteral("type")] = QStringLiteral("AudioStreamStart");
        startMsg[QStringLiteral("streamId")] = streamId;
        startMsg[QStringLiteral("codec")] = QStringLiteral("opus");
        startMsg[QStringLiteral("sampleRate")] = 16000;
        server.sendToClient(startMsg);
        QTest::qWait(200);

        // Server sends a valid Opus-encoded binary frame with the stream-ID prefix.
        QByteArray opus = encodeSilenceFrame();
        QVERIFY(!opus.isEmpty());
        QByteArray binaryFrame = pairion::protocol::BinaryCodec::encodeBinaryFrame(streamId, opus);
        server.sendBinaryToClient(binaryFrame);
        QTest::qWait(400);

        QCOMPARE(connState.agentState(), QStringLiteral("speaking"));

        wsClient.disconnectFromServer();
    }

    /// Verify agentState transitions to "idle" after AudioStreamEnd with reason "normal".
    void inboundStreamEndNormalSetsAgentStateIdle() {
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

        const QString streamId = QStringLiteral("aaaabbbb-cccc-dddd-eeee-ffffffffffff");

        // AudioStreamStart → binary frame → AudioStreamEnd.
        QJsonObject startMsg;
        startMsg[QStringLiteral("type")] = QStringLiteral("AudioStreamStart");
        startMsg[QStringLiteral("streamId")] = streamId;
        startMsg[QStringLiteral("codec")] = QStringLiteral("opus");
        startMsg[QStringLiteral("sampleRate")] = 16000;
        server.sendToClient(startMsg);

        QByteArray opus = encodeSilenceFrame();
        server.sendBinaryToClient(
            pairion::protocol::BinaryCodec::encodeBinaryFrame(streamId, opus));
        QTest::qWait(400);

        QJsonObject endMsg;
        endMsg[QStringLiteral("type")] = QStringLiteral("AudioStreamEnd");
        endMsg[QStringLiteral("streamId")] = streamId;
        endMsg[QStringLiteral("reason")] = QStringLiteral("normal");
        server.sendToClient(endMsg);
        QTest::qWait(200);

        QCOMPARE(connState.agentState(), QStringLiteral("idle"));

        wsClient.disconnectFromServer();
    }

    /// Verify preparePlayback() is safe to call even without audio hardware.
    void preparePlaybackDoesNotCrash() {
        PairionAudioPlayback playback;
        playback.preparePlayback(); // m_audioDevice may be null on headless CI — must not crash
    }

    /// Verify handleStreamEnd with a non-normal reason emits playbackError.
    void handleStreamEndAbnormalEmitsError() {
        PairionAudioPlayback playback;
        QSignalSpy errorSpy(&playback, &PairionAudioPlayback::playbackError);
        playback.handleStreamEnd(QStringLiteral("timeout"));
        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(errorSpy.at(0).at(0).toString().contains(QLatin1String("timeout")));
    }

    /// Verify handleStreamEnd("normal") does not emit playbackError.
    void handleStreamEndNormalNoError() {
        PairionAudioPlayback playback;
        QSignalSpy errorSpy(&playback, &PairionAudioPlayback::playbackError);
        playback.handleStreamEnd(QStringLiteral("normal"));
        QCOMPARE(errorSpy.count(), 0);
    }

    /// Verify start() is a no-op when not in StoppedState, and restarts when stopped.
    void startRestartsBehavior() {
        PairionAudioPlayback playback;
        playback.start();  // sink is IdleState from constructor — no-op
        playback.start();  // still IdleState — no-op; no crash
        playback.stop();   // sink now StoppedState, m_audioDevice nulled
        playback.start();  // StoppedState → restarts sink; no crash
        playback.stop();   // clean up
    }

    /// Verify stop() is safe to call when already stopped.
    void stopIdempotent() {
        PairionAudioPlayback playback;
        playback.stop();
        playback.stop(); // second call — already stopped; no crash
    }

    /// Verify preparePlayback() after stop() restarts the sink (covers the StoppedState branch).
    void preparePlaybackAfterStopRestartsDevice() {
        PairionAudioPlayback playback;
        playback.stop(); // explicitly stop the sink
        // preparePlayback() must restart the stopped sink without crashing.
        playback.preparePlayback();
        // A PCM frame delivered now should set m_isSpeaking=true regardless of hardware.
        QSignalSpy stateSpy(&playback, &PairionAudioPlayback::speakingStateChanged);
        playback.handlePcmFrame(QByteArray(640, '\0'));
        QCOMPARE(stateSpy.count(), 1);
        QCOMPARE(stateSpy.at(0).at(0).toString(), QStringLiteral("speaking"));
    }

    /// Verify the silence timer transitions speakingState to "idle" after 500 ms of no frames.
    void silenceTimerTransitionsToIdle() {
        PairionAudioPlayback playback;
        QSignalSpy stateSpy(&playback, &PairionAudioPlayback::speakingStateChanged);

        // Deliver one silence PCM frame — sets m_isSpeaking=true and starts the 500 ms timer.
        playback.handlePcmFrame(QByteArray(640, '\0'));
        QVERIFY(stateSpy.count() == 1);
        QCOMPARE(stateSpy.at(0).at(0).toString(), QStringLiteral("speaking"));

        // Wait past the 500 ms silence timer.
        QTest::qWait(600);
        QCOMPARE(stateSpy.count(), 2);
        QCOMPARE(stateSpy.at(1).at(0).toString(), QStringLiteral("idle"));
    }

    /// Verify a truncated binary frame (< 4 bytes) is silently dropped and agentState unchanged.
    void truncatedBinaryFrameDropped() {
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

        // Send a 2-byte frame — decodeBinaryFrame returns empty payload; onInboundAudio drops it.
        server.sendBinaryToClient(QByteArray(2, '\0'));
        QTest::qWait(200);

        QVERIFY(connState.agentState() != QStringLiteral("speaking"));

        wsClient.disconnectFromServer();
    }
};

QTEST_GUILESS_MAIN(TestInboundPlayback)
#include "tst_inbound_playback.moc"
