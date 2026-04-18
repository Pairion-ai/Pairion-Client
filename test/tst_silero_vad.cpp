/**
 * @file tst_silero_vad.cpp
 * @brief Tests for SileroVad using mock ONNX session.
 */

#include "../src/vad/silero_vad.h"
#include "mock_onnx_session.h"

#include <QSignalSpy>
#include <QTest>

using namespace pairion::vad;
using namespace pairion::test;
using namespace pairion::core;

/// Helper to create a mock output with speech probability and h/c state.
static std::vector<OnnxOutput> vadOutput(float speechProb) {
    return {OnnxOutput{{speechProb}, {1, 1}}, OnnxOutput{std::vector<float>(128, 0.0f), {2, 1, 64}},
            OnnxOutput{std::vector<float>(128, 0.0f), {2, 1, 64}}};
}

class TestSileroVad : public QObject {
    Q_OBJECT

  private slots:
    /// Verify silence does not emit speechStarted.
    void silenceNoSpeech() {
        MockOnnxSession session;
        // VAD accumulates 512 samples = 1024 bytes = ~1.6 frames of 640 bytes
        // Need 2 frames (1280 bytes) to trigger one inference
        session.enqueueOutput(vadOutput(0.1f));

        SileroVad vad(&session, 0.5, 800);
        QSignalSpy startSpy(&vad, &SileroVad::speechStarted);

        vad.processPcmFrame(QByteArray(640, '\0'));
        vad.processPcmFrame(QByteArray(640, '\0'));

        QCOMPARE(startSpy.count(), 0);
    }

    /// Verify speech probability above threshold emits speechStarted.
    void speechStartedEmitted() {
        MockOnnxSession session;
        session.enqueueOutput(vadOutput(0.9f));

        SileroVad vad(&session, 0.5, 800);
        QSignalSpy startSpy(&vad, &SileroVad::speechStarted);

        vad.processPcmFrame(QByteArray(640, '\0'));
        vad.processPcmFrame(QByteArray(640, '\0'));

        QCOMPARE(startSpy.count(), 1);
    }

    /// Verify speechEnded emitted after sustained silence.
    void speechEndedAfterSilence() {
        MockOnnxSession session;
        // First inference: speech
        session.enqueueOutput(vadOutput(0.9f));
        // Subsequent inferences: silence
        for (int i = 0; i < 50; ++i) {
            session.enqueueOutput(vadOutput(0.1f));
        }

        SileroVad vad(&session, 0.5, 200); // 200ms silence for faster test
        QSignalSpy startSpy(&vad, &SileroVad::speechStarted);
        QSignalSpy endSpy(&vad, &SileroVad::speechEnded);

        // First pair triggers speech start
        vad.processPcmFrame(QByteArray(640, '\0'));
        vad.processPcmFrame(QByteArray(640, '\0'));
        QCOMPARE(startSpy.count(), 1);

        // Feed more silence frames and wait for silence timer
        for (int i = 0; i < 20; ++i) {
            vad.processPcmFrame(QByteArray(640, '\0'));
            if (endSpy.count() > 0) {
                break;
            }
            QTest::qWait(50);
        }

        QVERIFY(endSpy.count() >= 1);
    }

    /// Verify speech resuming during trailing silence cancels end.
    void bargeBackInCancelsEnd() {
        MockOnnxSession session;
        // Speech
        session.enqueueOutput(vadOutput(0.9f));
        // Silence (enters trailing)
        session.enqueueOutput(vadOutput(0.1f));
        // Speech again (back to speaking)
        session.enqueueOutput(vadOutput(0.9f));

        SileroVad vad(&session, 0.5, 5000); // Long silence threshold
        QSignalSpy startSpy(&vad, &SileroVad::speechStarted);
        QSignalSpy endSpy(&vad, &SileroVad::speechEnded);

        // 6 frames = 3 inferences
        for (int i = 0; i < 6; ++i) {
            vad.processPcmFrame(QByteArray(640, '\0'));
        }

        QCOMPARE(startSpy.count(), 1);
        QCOMPARE(endSpy.count(), 0); // should not have ended
    }

    /// Verify reset clears state.
    void resetClearsState() {
        MockOnnxSession session;
        session.enqueueOutput(vadOutput(0.9f));

        SileroVad vad(&session, 0.5, 800);
        QSignalSpy startSpy(&vad, &SileroVad::speechStarted);

        vad.processPcmFrame(QByteArray(640, '\0'));
        vad.processPcmFrame(QByteArray(640, '\0'));
        QCOMPARE(startSpy.count(), 1);

        vad.reset();

        // After reset, speech detection should re-trigger from Idle
        session.enqueueOutput(vadOutput(0.9f));
        vad.processPcmFrame(QByteArray(640, '\0'));
        vad.processPcmFrame(QByteArray(640, '\0'));
        QCOMPARE(startSpy.count(), 2);
    }

    /// Verify h and c state is updated between inferences.
    void recurrentStateUpdated() {
        MockOnnxSession session;
        // Return non-zero h/c state
        std::vector<float> customState(128, 0.42f);
        session.enqueueOutput({OnnxOutput{{0.1f}, {1, 1}}, OnnxOutput{customState, {2, 1, 64}},
                               OnnxOutput{customState, {2, 1, 64}}});
        session.enqueueOutput(vadOutput(0.1f));

        SileroVad vad(&session, 0.5, 800);

        // First inference
        vad.processPcmFrame(QByteArray(640, '\0'));
        vad.processPcmFrame(QByteArray(640, '\0'));

        // Second inference — check that the mock received the updated state
        vad.processPcmFrame(QByteArray(640, '\0'));
        vad.processPcmFrame(QByteArray(640, '\0'));

        QCOMPARE(session.runCount(), 2);
        // The second call should have received the custom h state from the first output
        const auto &lastInputs = session.lastInputs();
        QVERIFY(lastInputs.size() >= 2);
        QCOMPARE(lastInputs[1].data[0], 0.42f); // h state should be 0.42
    }
};

QTEST_GUILESS_MAIN(TestSileroVad)
#include "tst_silero_vad.moc"
