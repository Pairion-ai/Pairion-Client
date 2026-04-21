/**
 * @file tst_connection_state.cpp
 * @brief Unit tests for ConnectionState, covering backgroundContext property:
 *        default value, setter, signal emission, and no-op on same value.
 */

#include "../src/state/connection_state.h"

#include <QSignalSpy>
#include <QTest>

using namespace pairion::state;

class TestConnectionState : public QObject {
    Q_OBJECT

  private slots:

    /// backgroundContext defaults to "earth".
    void backgroundContextDefault() {
        ConnectionState cs;
        QCOMPARE(cs.backgroundContext(), QStringLiteral("earth"));
    }

    /// setBackgroundContext stores the new value and returns it via getter.
    void backgroundContextSetAndGet() {
        ConnectionState cs;
        cs.setBackgroundContext(QStringLiteral("space"));
        QCOMPARE(cs.backgroundContext(), QStringLiteral("space"));
    }

    /// setBackgroundContext emits backgroundContextChanged exactly once on change.
    void backgroundContextEmitsSignal() {
        ConnectionState cs;
        QSignalSpy spy(&cs, &ConnectionState::backgroundContextChanged);
        cs.setBackgroundContext(QStringLiteral("space"));
        QCOMPARE(spy.count(), 1);
    }

    /// setBackgroundContext does not emit when value is unchanged.
    void backgroundContextNoEmitOnSameValue() {
        ConnectionState cs;
        // default is "earth" — set to "earth" again
        QSignalSpy spy(&cs, &ConnectionState::backgroundContextChanged);
        cs.setBackgroundContext(QStringLiteral("earth"));
        QCOMPARE(spy.count(), 0);
    }

    /// setBackgroundContext round-trips through multiple context values.
    void backgroundContextRoundTrip() {
        ConnectionState cs;
        QSignalSpy spy(&cs, &ConnectionState::backgroundContextChanged);

        cs.setBackgroundContext(QStringLiteral("space"));
        cs.setBackgroundContext(QStringLiteral("earth"));
        cs.setBackgroundContext(QStringLiteral("space"));

        QCOMPARE(spy.count(), 3);
        QCOMPARE(cs.backgroundContext(), QStringLiteral("space"));
    }
};

QTEST_GUILESS_MAIN(TestConnectionState)
#include "tst_connection_state.moc"
