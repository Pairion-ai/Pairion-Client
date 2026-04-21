/**
 * @file tst_connection_state.cpp
 * @brief Unit tests for ConnectionState, covering backgroundContext and mapFocus properties:
 *        default values, setters, signal emission, and no-op on same value.
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

    /// mapFocusActive defaults to false.
    void mapFocusDefaultInactive() {
        ConnectionState cs;
        QCOMPARE(cs.mapFocusActive(), false);
    }

    /// setMapFocus activates focus and stores all fields.
    void mapFocusSetAndGet() {
        ConnectionState cs;
        cs.setMapFocus(35.6762, 139.6503, QStringLiteral("Tokyo, Japan"),
                       QStringLiteral("city"));
        QCOMPARE(cs.mapFocusActive(), true);
        QCOMPARE(cs.mapFocusLat(), 35.6762);
        QCOMPARE(cs.mapFocusLon(), 139.6503);
        QCOMPARE(cs.mapFocusLabel(), QStringLiteral("Tokyo, Japan"));
        QCOMPARE(cs.mapFocusZoom(), QStringLiteral("city"));
    }

    /// setMapFocus emits mapFocusChanged.
    void mapFocusEmitsSignal() {
        ConnectionState cs;
        QSignalSpy spy(&cs, &ConnectionState::mapFocusChanged);
        cs.setMapFocus(35.6762, 139.6503, QStringLiteral("Tokyo, Japan"),
                       QStringLiteral("city"));
        QCOMPARE(spy.count(), 1);
    }

    /// clearMapFocus deactivates the focus and emits mapFocusChanged.
    void mapFocusClearDeactivates() {
        ConnectionState cs;
        cs.setMapFocus(35.6762, 139.6503, QStringLiteral("Tokyo, Japan"),
                       QStringLiteral("city"));
        QSignalSpy spy(&cs, &ConnectionState::mapFocusChanged);
        cs.clearMapFocus();
        QCOMPARE(cs.mapFocusActive(), false);
        QCOMPARE(spy.count(), 1);
    }

    /// clearMapFocus does not emit when already inactive.
    void mapFocusClearNoEmitWhenAlreadyInactive() {
        ConnectionState cs;
        QSignalSpy spy(&cs, &ConnectionState::mapFocusChanged);
        cs.clearMapFocus();
        QCOMPARE(spy.count(), 0);
    }
};

QTEST_GUILESS_MAIN(TestConnectionState)
#include "tst_connection_state.moc"
