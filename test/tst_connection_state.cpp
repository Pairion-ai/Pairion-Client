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

    // ── Scene property tests ──────────────────────────────────────

    /// activeSceneId defaults to "globe".
    void activeSceneIdDefault() {
        ConnectionState cs;
        QCOMPARE(cs.activeSceneId(), QStringLiteral("globe"));
    }

    /// setActiveSceneId stores the new value.
    void activeSceneIdSetAndGet() {
        ConnectionState cs;
        cs.setActiveSceneId(QStringLiteral("space"));
        QCOMPARE(cs.activeSceneId(), QStringLiteral("space"));
    }

    /// setActiveSceneId emits activeSceneIdChanged on change.
    void activeSceneIdEmitsSignal() {
        ConnectionState cs;
        QSignalSpy spy(&cs, &ConnectionState::activeSceneIdChanged);
        cs.setActiveSceneId(QStringLiteral("space"));
        QCOMPARE(spy.count(), 1);
    }

    /// setActiveSceneId does not emit when value is unchanged.
    void activeSceneIdNoEmitOnSameValue() {
        ConnectionState cs;
        QSignalSpy spy(&cs, &ConnectionState::activeSceneIdChanged);
        cs.setActiveSceneId(QStringLiteral("globe"));
        QCOMPARE(spy.count(), 0);
    }

    /// sceneData is empty by default.
    void sceneDataDefault() {
        ConnectionState cs;
        QVERIFY(cs.sceneData().isEmpty());
    }

    /// setSceneData stores data under the given modelId and emits sceneDataChanged.
    void sceneDataSetAndGet() {
        ConnectionState cs;
        QSignalSpy spy(&cs, &ConnectionState::sceneDataChanged);

        QVariantMap data;
        data[QStringLiteral("headline")] = QStringLiteral("Market Volatility");
        cs.setSceneData(QStringLiteral("news"), data);

        QCOMPARE(spy.count(), 1);
        QVERIFY(cs.sceneData().contains(QStringLiteral("news")));
        QVariantMap stored = cs.sceneData()[QStringLiteral("news")].toMap();
        QCOMPARE(stored[QStringLiteral("headline")].toString(),
                 QStringLiteral("Market Volatility"));
    }

    /// setSceneData accumulates multiple model IDs.
    void sceneDataAccumulates() {
        ConnectionState cs;
        QVariantMap d1; d1[QStringLiteral("x")] = 1;
        QVariantMap d2; d2[QStringLiteral("y")] = 2;
        cs.setSceneData(QStringLiteral("modelA"), d1);
        cs.setSceneData(QStringLiteral("modelB"), d2);
        QCOMPARE(cs.sceneData().size(), 2);
    }

    /// clearSceneData empties the map and emits sceneDataChanged.
    void sceneDataClear() {
        ConnectionState cs;
        QVariantMap data; data[QStringLiteral("k")] = QStringLiteral("v");
        cs.setSceneData(QStringLiteral("m"), data);

        QSignalSpy spy(&cs, &ConnectionState::sceneDataChanged);
        cs.clearSceneData();

        QCOMPARE(spy.count(), 1);
        QVERIFY(cs.sceneData().isEmpty());
    }

    /// clearSceneData does not emit when already empty.
    void sceneDataClearNoEmitWhenEmpty() {
        ConnectionState cs;
        QSignalSpy spy(&cs, &ConnectionState::sceneDataChanged);
        cs.clearSceneData();
        QCOMPARE(spy.count(), 0);
    }

    /// sceneParams defaults to an empty map.
    void sceneParamsDefault() {
        ConnectionState cs;
        QVERIFY(cs.sceneParams().isEmpty());
    }

    /// setSceneParams stores params and emits sceneParamsChanged.
    void sceneParamsSetAndGet() {
        ConnectionState cs;
        QSignalSpy spy(&cs, &ConnectionState::sceneParamsChanged);

        QVariantMap params;
        params[QStringLiteral("focus")] = QStringLiteral("dallas");
        cs.setSceneParams(params);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(cs.sceneParams()[QStringLiteral("focus")].toString(),
                 QStringLiteral("dallas"));
    }

    /// setSceneParams does not emit when value is unchanged.
    void sceneParamsNoEmitOnSameValue() {
        ConnectionState cs;
        QVariantMap params; params[QStringLiteral("a")] = 1;
        cs.setSceneParams(params);

        QSignalSpy spy(&cs, &ConnectionState::sceneParamsChanged);
        cs.setSceneParams(params);
        QCOMPARE(spy.count(), 0);
    }

    /// sceneTransition defaults to "crossfade".
    void sceneTransitionDefault() {
        ConnectionState cs;
        QCOMPARE(cs.sceneTransition(), QStringLiteral("crossfade"));
    }

    /// setSceneTransition stores the value and emits sceneTransitionChanged.
    void sceneTransitionSetAndGet() {
        ConnectionState cs;
        QSignalSpy spy(&cs, &ConnectionState::sceneTransitionChanged);
        cs.setSceneTransition(QStringLiteral("instant"));
        QCOMPARE(cs.sceneTransition(), QStringLiteral("instant"));
        QCOMPARE(spy.count(), 1);
    }

    /// setSceneTransition does not emit when value is unchanged.
    void sceneTransitionNoEmitOnSameValue() {
        ConnectionState cs;
        QSignalSpy spy(&cs, &ConnectionState::sceneTransitionChanged);
        cs.setSceneTransition(QStringLiteral("crossfade"));
        QCOMPARE(spy.count(), 0);
    }
};

QTEST_GUILESS_MAIN(TestConnectionState)
#include "tst_connection_state.moc"
