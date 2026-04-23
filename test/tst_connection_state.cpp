/**
 * @file tst_connection_state.cpp
 * @brief Unit tests for ConnectionState, covering backgroundContext and mapFocus properties:
 *        default values, setters, signal emission, and no-op on same value.
 */

#include "../src/state/connection_state.h"

#include <QJsonObject>
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

    // ── Layer property tests ──────────────────────────────────────

    /// activeBackgroundId defaults to "globe".
    void activeBackgroundIdDefault() {
        ConnectionState cs;
        QCOMPARE(cs.activeBackgroundId(), QStringLiteral("globe"));
    }

    /// setActiveBackgroundId stores the new value.
    void activeBackgroundIdSetAndGet() {
        ConnectionState cs;
        cs.setActiveBackgroundId(QStringLiteral("space"));
        QCOMPARE(cs.activeBackgroundId(), QStringLiteral("space"));
    }

    /// setActiveBackgroundId emits activeBackgroundIdChanged on change.
    void activeBackgroundIdEmitsSignal() {
        ConnectionState cs;
        QSignalSpy spy(&cs, &ConnectionState::activeBackgroundIdChanged);
        cs.setActiveBackgroundId(QStringLiteral("space"));
        QCOMPARE(spy.count(), 1);
    }

    /// setActiveBackgroundId does not emit when value is unchanged.
    void activeBackgroundIdNoEmitOnSameValue() {
        ConnectionState cs;
        QSignalSpy spy(&cs, &ConnectionState::activeBackgroundIdChanged);
        cs.setActiveBackgroundId(QStringLiteral("globe"));
        QCOMPARE(spy.count(), 0);
    }

    /// activeOverlayIds is empty by default.
    void activeOverlayIdsDefault() {
        ConnectionState cs;
        QVERIFY(cs.activeOverlayIds().isEmpty());
    }

    /// addOverlay appends overlayId and emits activeOverlayIdsChanged.
    void addOverlayAppendsId() {
        ConnectionState cs;
        QSignalSpy spy(&cs, &ConnectionState::activeOverlayIdsChanged);
        cs.addOverlay(QStringLiteral("adsb"), QJsonObject());
        QCOMPARE(cs.activeOverlayIds().size(), 1);
        QCOMPARE(cs.activeOverlayIds().at(0), QStringLiteral("adsb"));
        QCOMPARE(spy.count(), 1);
    }

    /// addOverlay does not duplicate an existing overlayId.
    void addOverlayNoDuplicate() {
        ConnectionState cs;
        cs.addOverlay(QStringLiteral("adsb"), QJsonObject());
        cs.addOverlay(QStringLiteral("adsb"), QJsonObject());
        QCOMPARE(cs.activeOverlayIds().size(), 1);
    }

    /// removeOverlay removes an existing overlay and emits activeOverlayIdsChanged.
    void removeOverlayRemovesId() {
        ConnectionState cs;
        cs.addOverlay(QStringLiteral("adsb"), QJsonObject());
        QSignalSpy spy(&cs, &ConnectionState::activeOverlayIdsChanged);
        cs.removeOverlay(QStringLiteral("adsb"));
        QVERIFY(cs.activeOverlayIds().isEmpty());
        QCOMPARE(spy.count(), 1);
    }

    /// removeOverlay does not emit when overlayId is not active.
    void removeOverlayNoEmitWhenAbsent() {
        ConnectionState cs;
        QSignalSpy spy(&cs, &ConnectionState::activeOverlayIdsChanged);
        cs.removeOverlay(QStringLiteral("adsb"));
        QCOMPARE(spy.count(), 0);
    }

    /// clearOverlays empties the overlay list and emits activeOverlayIdsChanged.
    void clearOverlaysClearsAll() {
        ConnectionState cs;
        cs.addOverlay(QStringLiteral("adsb"), QJsonObject());
        cs.addOverlay(QStringLiteral("weather_current"), QJsonObject());
        QSignalSpy spy(&cs, &ConnectionState::activeOverlayIdsChanged);
        cs.clearOverlays();
        QVERIFY(cs.activeOverlayIds().isEmpty());
        QCOMPARE(spy.count(), 1);
    }

    /// clearOverlays does not emit when already empty.
    void clearOverlaysNoEmitWhenEmpty() {
        ConnectionState cs;
        QSignalSpy spy(&cs, &ConnectionState::activeOverlayIdsChanged);
        cs.clearOverlays();
        QCOMPARE(spy.count(), 0);
    }

    /// setBackground sets activeBackgroundId and emits both background signals.
    void setBackgroundUpdatesAllFields() {
        ConnectionState cs;
        QSignalSpy bgSpy(&cs, &ConnectionState::activeBackgroundIdChanged);
        QSignalSpy paramSpy(&cs, &ConnectionState::backgroundParamsChanged);
        QSignalSpy transSpy(&cs, &ConnectionState::sceneTransitionChanged);

        QJsonObject params;
        params[QStringLiteral("center_lat")] = 33.814;
        cs.setBackground(QStringLiteral("vfr"), params, QStringLiteral("instant"));

        QCOMPARE(cs.activeBackgroundId(), QStringLiteral("vfr"));
        QCOMPARE(cs.backgroundParams()[QStringLiteral("center_lat")].toDouble(), 33.814);
        QCOMPARE(cs.sceneTransition(), QStringLiteral("instant"));
        QCOMPARE(bgSpy.count(), 1);
        QCOMPARE(paramSpy.count(), 1);
        QCOMPARE(transSpy.count(), 1);
    }

    /// overlayParams is empty by default.
    void overlayParamsDefault() {
        ConnectionState cs;
        QVERIFY(cs.overlayParams().isEmpty());
    }

    /// addOverlay stores params keyed by overlayId in overlayParams.
    void addOverlayStoresParams() {
        ConnectionState cs;
        QJsonObject params;
        params[QStringLiteral("radius_nm")] = 8;
        cs.addOverlay(QStringLiteral("adsb"), params);
        QVariantMap stored = cs.overlayParams()[QStringLiteral("adsb")].toMap();
        QCOMPARE(stored[QStringLiteral("radius_nm")].toInt(), 8);
    }

    /// sceneData is empty by default.
    void sceneDataDefault() {
        ConnectionState cs;
        QVERIFY(cs.sceneData().isEmpty());
    }

    /// setSceneData stores a QVariantMap payload under the given modelId and emits sceneDataChanged.
    void sceneDataSetAndGet() {
        ConnectionState cs;
        QSignalSpy spy(&cs, &ConnectionState::sceneDataChanged);

        QVariantMap data;
        data[QStringLiteral("headline")] = QStringLiteral("Market Volatility");
        cs.setSceneData(QStringLiteral("news"), QVariant(data));

        QCOMPARE(spy.count(), 1);
        QVERIFY(cs.sceneData().contains(QStringLiteral("news")));
        QVariantMap stored = cs.sceneData()[QStringLiteral("news")].toMap();
        QCOMPARE(stored[QStringLiteral("headline")].toString(),
                 QStringLiteral("Market Volatility"));
    }

    /// setSceneData stores a QVariantList (JSON array) payload — required by ADS-B scene.
    void sceneDataSetAndGetList() {
        ConnectionState cs;
        QSignalSpy spy(&cs, &ConnectionState::sceneDataChanged);

        QVariantMap ac1;
        ac1[QStringLiteral("icao24")]   = QStringLiteral("abc123");
        ac1[QStringLiteral("callsign")] = QStringLiteral("UAL123");
        QVariantList list;
        list.append(ac1);

        cs.setSceneData(QStringLiteral("adsb"), QVariant(list));

        QCOMPARE(spy.count(), 1);
        QVERIFY(cs.sceneData().contains(QStringLiteral("adsb")));
        QVariantList stored = cs.sceneData()[QStringLiteral("adsb")].toList();
        QCOMPARE(stored.size(), 1);
        QCOMPARE(stored[0].toMap()[QStringLiteral("icao24")].toString(),
                 QStringLiteral("abc123"));
    }

    /// setSceneData accumulates multiple model IDs.
    void sceneDataAccumulates() {
        ConnectionState cs;
        QVariantMap d1; d1[QStringLiteral("x")] = 1;
        QVariantMap d2; d2[QStringLiteral("y")] = 2;
        cs.setSceneData(QStringLiteral("modelA"), QVariant(d1));
        cs.setSceneData(QStringLiteral("modelB"), QVariant(d2));
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

    // ── pipelineHealth property tests ─────────────────────────────────────────

    /// pipelineHealth defaults to "connecting".
    void pipelineHealthDefault() {
        ConnectionState cs;
        QCOMPARE(cs.pipelineHealth(), QStringLiteral("connecting"));
    }

    /// setPipelineHealth stores the new value.
    void pipelineHealthSetAndGet() {
        ConnectionState cs;
        cs.setPipelineHealth(QStringLiteral("ready"));
        QCOMPARE(cs.pipelineHealth(), QStringLiteral("ready"));
    }

    /// setPipelineHealth emits pipelineHealthChanged exactly once on change.
    void pipelineHealthEmitsSignal() {
        ConnectionState cs;
        QSignalSpy spy(&cs, &ConnectionState::pipelineHealthChanged);
        cs.setPipelineHealth(QStringLiteral("ready"));
        QCOMPARE(spy.count(), 1);
    }

    /// setPipelineHealth does not emit when value is unchanged.
    void pipelineHealthNoEmitOnSameValue() {
        ConnectionState cs;
        QSignalSpy spy(&cs, &ConnectionState::pipelineHealthChanged);
        cs.setPipelineHealth(QStringLiteral("connecting"));
        QCOMPARE(spy.count(), 0);
    }

    /// setPipelineHealth round-trips through all defined health states.
    void pipelineHealthRoundTrip() {
        ConnectionState cs;
        QSignalSpy spy(&cs, &ConnectionState::pipelineHealthChanged);

        const QStringList states = {QStringLiteral("models_loading"),
                                    QStringLiteral("initializing"),
                                    QStringLiteral("ready"),
                                    QStringLiteral("mic_offline"),
                                    QStringLiteral("wake_failed"),
                                    QStringLiteral("server_disconnected"),
                                    QStringLiteral("models_failed"),
                                    QStringLiteral("pipeline_error")};
        for (const auto &state : states) {
            cs.setPipelineHealth(state);
        }

        QCOMPARE(spy.count(), states.size());
        QCOMPARE(cs.pipelineHealth(), QStringLiteral("pipeline_error"));
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
