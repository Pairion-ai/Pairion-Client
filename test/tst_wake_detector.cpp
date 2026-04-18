/**
 * @file tst_wake_detector.cpp
 * @brief Tests for OpenWakewordDetector using mock ONNX sessions.
 *
 * The real pipeline needs many frames to accumulate enough mel/embedding data.
 * Tests inject properly-shaped mock outputs at each stage to exercise the
 * threshold, suppression, and pre-roll logic without requiring real audio.
 */

#include "../src/wake/open_wakeword_detector.h"
#include "mock_onnx_session.h"

#include <QSignalSpy>
#include <QTest>

using namespace pairion::wake;
using namespace pairion::test;
using namespace pairion::core;

/// Helper: create a mel output with 5 frames of 32 floats = 160 floats, shape [1,1,5,32].
static std::vector<OnnxOutput> melOutput() {
    return {{OnnxOutput{std::vector<float>(160, 0.1f), {1, 1, 5, 32}}}};
}

/// Helper: create an embedding output [1,1,1,96] = 96 floats.
static std::vector<OnnxOutput> embOutput() {
    return {{OnnxOutput{std::vector<float>(96, 0.1f), {1, 1, 1, 96}}}};
}

/// Helper: create a classifier output with the given score.
static std::vector<OnnxOutput> clsOutput(float score) {
    return {{OnnxOutput{{score}, {1, 1}}}};
}

/// Feed enough frames to trigger one full classification cycle.
/// Need: 4 frames (2560 bytes) per mel chunk, 76 mel frames = ~16 mel chunks,
/// 16 embedding features = 16 * 76 mel frames = lots.
/// For testing, we pre-fill the mel and embedding queues with enough outputs
/// and feed enough PCM to trigger the pipeline through to classification.
static void feedFramesForOneClassification(OpenWakewordDetector &detector, MockOnnxSession &mel,
                                           MockOnnxSession &emb, MockOnnxSession &cls,
                                           float clsScore) {
    // Each 2560-byte chunk (4 x 640-byte frames) triggers one mel inference
    // which produces 5 mel frames. We need 76 mel frames → 16 mel calls.
    // Then one embedding call, which needs 16 to accumulate → 16 embedding calls.
    // Then one classifier call.
    //
    // For a simpler test: feed exactly 4 frames (one mel chunk) but pre-fill
    // the mel frame and embedding feature deques via sequential mock outputs.
    //
    // Strategy: enqueue enough mel outputs so that after processing, we have ≥76
    // mel frames accumulated. Then one embedding output. Repeat until 16
    // embeddings accumulated. Then one classifier output.
    //
    // Simplification: since the mock returns canned outputs regardless of input,
    // and the detector accumulates mel frames across calls, we feed 4*16 = 64
    // frames (16 mel chunks producing 80 mel frames) and enqueue 16 mel outputs.
    // After the 16th mel chunk, we have 80 mel frames (≥76), so embedding runs.
    // We need 16 embedding calls, so we do this 16 times total = 16*16 mel chunks
    // = 256 mel chunks = 1024 frames. That's too many.
    //
    // Better: just enqueue mel outputs that produce enough frames per call.
    // Each mel call adds 5 frames to the deque. We need 76 frames.
    // After 16 mel calls (64 frames of PCM = 16*4=64), we have 80 mel frames → embedding runs.
    // But we need 16 embeddings. Each embedding needs 76 mel frames. After first
    // embedding runs, mel frames deque is still ~80 (trimmed to 76 last). Next 5 mel
    // frames push it to 81, trimmed to 76 → another embedding. So each mel chunk
    // after the first 16 triggers another embedding.
    //
    // We need 16 embeddings → 16 additional mel chunks → 16*4 = 64 more frames.
    // Plus initial 64 frames. Total = 128 frames. Doable but lots of enqueuing.
    //
    // Simplest practical approach: enqueue all mock outputs upfront, then feed
    // enough frames to trigger the pipeline.

    // We need: 16 (initial) + 15 (subsequent) = 31 mel calls, 16 emb calls, 1 cls call
    for (int i = 0; i < 31; ++i) {
        mel.enqueueOutput(melOutput());
    }
    for (int i = 0; i < 16; ++i) {
        emb.enqueueOutput(embOutput());
    }
    cls.enqueueOutput(clsOutput(clsScore));

    // Feed 31*4 = 124 frames of 640 bytes each (31 mel chunks)
    QByteArray pcm(640, '\0');
    for (int i = 0; i < 124; ++i) {
        detector.processPcmFrame(pcm);
    }
}

class TestWakeDetector : public QObject {
    Q_OBJECT

  private slots:
    /// Verify score below threshold does not fire.
    void belowThresholdDoesNotFire() {
        MockOnnxSession mel, emb, cls;
        OpenWakewordDetector detector(&mel, &emb, &cls, 0.5);
        QSignalSpy spy(&detector, &OpenWakewordDetector::wakeWordDetected);

        feedFramesForOneClassification(detector, mel, emb, cls, 0.3f);
        QCOMPARE(spy.count(), 0);
    }

    /// Verify score above threshold fires.
    void aboveThresholdFires() {
        MockOnnxSession mel, emb, cls;
        OpenWakewordDetector detector(&mel, &emb, &cls, 0.5);
        QSignalSpy spy(&detector, &OpenWakewordDetector::wakeWordDetected);

        feedFramesForOneClassification(detector, mel, emb, cls, 0.9f);
        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toFloat() >= 0.9f);
    }

    /// Verify suppression prevents immediate refire.
    void suppressionPreventsRefire() {
        MockOnnxSession mel, emb, cls;
        OpenWakewordDetector detector(&mel, &emb, &cls, 0.5);
        QSignalSpy spy(&detector, &OpenWakewordDetector::wakeWordDetected);

        feedFramesForOneClassification(detector, mel, emb, cls, 0.9f);
        QCOMPARE(spy.count(), 1);

        // Immediately feed another classification cycle — should be suppressed
        feedFramesForOneClassification(detector, mel, emb, cls, 0.9f);
        QCOMPARE(spy.count(), 1);
    }

    /// Verify suppression expires after the window.
    void suppressionExpires() {
        MockOnnxSession mel, emb, cls;
        OpenWakewordDetector detector(&mel, &emb, &cls, 0.5);
        QSignalSpy spy(&detector, &OpenWakewordDetector::wakeWordDetected);

        feedFramesForOneClassification(detector, mel, emb, cls, 0.9f);
        QCOMPARE(spy.count(), 1);

        QTest::qWait(550); // Wait past 500ms suppression

        feedFramesForOneClassification(detector, mel, emb, cls, 0.9f);
        QCOMPARE(spy.count(), 2);
    }

    /// Verify pre-roll buffer is attached and non-empty.
    void preRollBufferAttached() {
        MockOnnxSession mel, emb, cls;
        OpenWakewordDetector detector(&mel, &emb, &cls, 0.5);
        QSignalSpy spy(&detector, &OpenWakewordDetector::wakeWordDetected);

        feedFramesForOneClassification(detector, mel, emb, cls, 0.9f);
        QCOMPARE(spy.count(), 1);
        QByteArray preRoll = spy.at(0).at(1).toByteArray();
        QVERIFY(!preRoll.isEmpty());
        QVERIFY(preRoll.size() <= 6400);
    }

    /// Verify mel session is called.
    void melSessionCalled() {
        MockOnnxSession mel, emb, cls;
        OpenWakewordDetector detector(&mel, &emb, &cls, 0.5);

        feedFramesForOneClassification(detector, mel, emb, cls, 0.1f);
        QVERIFY(mel.runCount() > 0);
        QVERIFY(emb.runCount() > 0);
        QVERIFY(cls.runCount() > 0);
    }
};

QTEST_GUILESS_MAIN(TestWakeDetector)
#include "tst_wake_detector.moc"
