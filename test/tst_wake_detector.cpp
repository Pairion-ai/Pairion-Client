/**
 * @file tst_wake_detector.cpp
 * @brief Tests for OpenWakewordDetector using mock ONNX sessions.
 */

#include "../src/wake/open_wakeword_detector.h"
#include "mock_onnx_session.h"

#include <QSignalSpy>
#include <QTest>

using namespace pairion::wake;
using namespace pairion::test;
using namespace pairion::core;

class TestWakeDetector : public QObject {
    Q_OBJECT

  private slots:
    /// Verify score below threshold does not fire.
    void belowThresholdDoesNotFire() {
        MockOnnxSession mel, emb, cls;
        // Classifier returns 0.3 (below 0.5 threshold)
        cls.enqueueOutput({{OnnxOutput{{0.3f}, {1}}}});

        OpenWakewordDetector detector(&mel, &emb, &cls, 0.5);
        QSignalSpy spy(&detector, &OpenWakewordDetector::wakeWordDetected);

        detector.processPcmFrame(QByteArray(640, '\0'));
        QCOMPARE(spy.count(), 0);
    }

    /// Verify score above threshold fires.
    void aboveThresholdFires() {
        MockOnnxSession mel, emb, cls;
        cls.enqueueOutput({{OnnxOutput{{0.9f}, {1}}}});

        OpenWakewordDetector detector(&mel, &emb, &cls, 0.5);
        QSignalSpy spy(&detector, &OpenWakewordDetector::wakeWordDetected);

        detector.processPcmFrame(QByteArray(640, '\0'));
        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toFloat() >= 0.9f);
    }

    /// Verify suppression prevents immediate refire.
    void suppressionPreventsRefire() {
        MockOnnxSession mel, emb, cls;
        cls.enqueueOutput({{OnnxOutput{{0.9f}, {1}}}});
        cls.enqueueOutput({{OnnxOutput{{0.9f}, {1}}}});

        OpenWakewordDetector detector(&mel, &emb, &cls, 0.5);
        QSignalSpy spy(&detector, &OpenWakewordDetector::wakeWordDetected);

        detector.processPcmFrame(QByteArray(640, '\0')); // fires
        detector.processPcmFrame(QByteArray(640, '\0')); // suppressed
        QCOMPARE(spy.count(), 1);
    }

    /// Verify suppression expires after the window.
    void suppressionExpires() {
        MockOnnxSession mel, emb, cls;
        cls.enqueueOutput({{OnnxOutput{{0.9f}, {1}}}});
        cls.enqueueOutput({{OnnxOutput{{0.9f}, {1}}}});

        OpenWakewordDetector detector(&mel, &emb, &cls, 0.5);
        QSignalSpy spy(&detector, &OpenWakewordDetector::wakeWordDetected);

        detector.processPcmFrame(QByteArray(640, '\0')); // fires
        QCOMPARE(spy.count(), 1);

        QTest::qWait(550); // wait past 500ms suppression

        detector.processPcmFrame(QByteArray(640, '\0')); // should fire again
        QCOMPARE(spy.count(), 2);
    }

    /// Verify pre-roll buffer is attached to the signal.
    void preRollBufferAttached() {
        MockOnnxSession mel, emb, cls;
        // First few frames below threshold, then one above
        for (int i = 0; i < 5; ++i) {
            cls.enqueueOutput({{OnnxOutput{{0.1f}, {1}}}});
        }
        cls.enqueueOutput({{OnnxOutput{{0.9f}, {1}}}});

        OpenWakewordDetector detector(&mel, &emb, &cls, 0.5);
        QSignalSpy spy(&detector, &OpenWakewordDetector::wakeWordDetected);

        for (int i = 0; i < 6; ++i) {
            detector.processPcmFrame(QByteArray(640, '\0'));
        }

        QCOMPARE(spy.count(), 1);
        QByteArray preRoll = spy.at(0).at(1).toByteArray();
        QVERIFY(!preRoll.isEmpty());
        QVERIFY(preRoll.size() <= 6400); // max ~200ms
    }

    /// Verify pre-roll buffer trims to kPreRollBytes.
    void preRollBufferTrims() {
        MockOnnxSession mel, emb, cls;
        // Feed 20 frames (20 * 640 = 12800 bytes) below threshold, then fire
        for (int i = 0; i < 20; ++i) {
            cls.enqueueOutput({{OnnxOutput{{0.1f}, {1}}}});
        }
        cls.enqueueOutput({{OnnxOutput{{0.9f}, {1}}}});

        OpenWakewordDetector detector(&mel, &emb, &cls, 0.5);
        QSignalSpy spy(&detector, &OpenWakewordDetector::wakeWordDetected);

        for (int i = 0; i < 21; ++i) {
            detector.processPcmFrame(QByteArray(640, '\0'));
        }

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).toByteArray().size(), 6400);
    }

    /// Verify all three ONNX sessions are called in sequence.
    void allSessionsCalled() {
        MockOnnxSession mel, emb, cls;
        cls.enqueueOutput({{OnnxOutput{{0.1f}, {1}}}});

        OpenWakewordDetector detector(&mel, &emb, &cls, 0.5);
        detector.processPcmFrame(QByteArray(640, '\0'));

        QCOMPARE(mel.runCount(), 1);
        QCOMPARE(emb.runCount(), 1);
        QCOMPARE(cls.runCount(), 1);
    }
};

QTEST_GUILESS_MAIN(TestWakeDetector)
#include "tst_wake_detector.moc"
