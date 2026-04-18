/**
 * @file tst_ring_buffer.cpp
 * @brief Tests for the lock-free SPSC RingBuffer template.
 */

#include "../src/audio/ring_buffer.h"

#include <QTest>

using namespace pairion::audio;

class TestRingBuffer : public QObject {
    Q_OBJECT

  private slots:
    /// Verify push and pop round-trip.
    void pushPopRoundTrip() {
        RingBuffer<int, 4> buf;
        QVERIFY(buf.push(42));
        int val = 0;
        QVERIFY(buf.pop(val));
        QCOMPARE(val, 42);
    }

    /// Verify buffer starts empty.
    void startsEmpty() {
        RingBuffer<int, 4> buf;
        QVERIFY(buf.isEmpty());
        QVERIFY(!buf.isFull());
        QCOMPARE(buf.size(), 0u);
    }

    /// Verify full buffer rejects push.
    void fullBufferRejectsPush() {
        RingBuffer<int, 4> buf;
        // Capacity 4 means 3 usable slots (one reserved for full/empty discrimination)
        QVERIFY(buf.push(1));
        QVERIFY(buf.push(2));
        QVERIFY(buf.push(3));
        QVERIFY(buf.isFull());
        QVERIFY(!buf.push(4)); // should fail
    }

    /// Verify empty buffer rejects pop.
    void emptyBufferRejectsPop() {
        RingBuffer<int, 4> buf;
        int val = 0;
        QVERIFY(!buf.pop(val));
    }

    /// Verify wrap-around works correctly.
    void wrapAround() {
        RingBuffer<int, 4> buf;
        // Fill and drain multiple times to wrap around
        for (int round = 0; round < 5; ++round) {
            QVERIFY(buf.push(round * 10 + 1));
            QVERIFY(buf.push(round * 10 + 2));
            int v1 = 0, v2 = 0;
            QVERIFY(buf.pop(v1));
            QVERIFY(buf.pop(v2));
            QCOMPARE(v1, round * 10 + 1);
            QCOMPARE(v2, round * 10 + 2);
        }
    }

    /// Verify size tracking.
    void sizeTracking() {
        RingBuffer<int, 8> buf;
        QCOMPARE(buf.size(), 0u);
        buf.push(1);
        QCOMPARE(buf.size(), 1u);
        buf.push(2);
        QCOMPARE(buf.size(), 2u);
        int v = 0;
        buf.pop(v);
        QCOMPARE(buf.size(), 1u);
    }

    /// Verify works with QByteArray elements.
    void worksWithQByteArray() {
        RingBuffer<QByteArray, 4> buf;
        QByteArray data(640, 'A');
        QVERIFY(buf.push(data));
        QByteArray out;
        QVERIFY(buf.pop(out));
        QCOMPARE(out, data);
    }
};

QTEST_GUILESS_MAIN(TestRingBuffer)
#include "tst_ring_buffer.moc"
