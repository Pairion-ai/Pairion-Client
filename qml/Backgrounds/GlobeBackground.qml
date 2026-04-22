/**
 * @file GlobeBackground.qml
 * @brief Globe background plugin — wraps HemisphereMap with server-driven focus and news pins.
 *
 * This background is the default (activeBackgroundId = "globe"). It renders the animated
 * day/night hemisphere map with five news-pin overlays and responds to
 * ConnectionState.mapFocusActive / mapFocusLat / mapFocusLon / mapFocusZoom.
 *
 * PairionHUD.focusPin() forwards pin-index changes through LayerManager.currentBackground
 * using the activePinIndex property exposed here.
 */
import QtQuick
import Pairion
import "../HUD"

Item {
    id: root

    // ── BackgroundBase contract ───────────────────────────────────

    /** @brief Parameters from the BackgroundChange command that activated this background. */
    property var backgroundParams: ({})
    /** @brief Model data keyed by modelId. */
    property var sceneData: ({})
    /** @brief Current agent state string from LayerManager. */
    property string hudState: "idle"

    /** @brief Default coordinate bridge — GlobeBackground is not geo-referenced. */
    function latLonToScreen(lat, lon) { return Qt.point(width * 0.5, height * 0.5) }

    /** @brief Request TTS narration. */
    signal requestSpeak(string text)

    // ── Globe-specific API ────────────────────────────────────────

    /**
     * @brief Zero-based index of the pin to focus, or -1 to resume auto-scroll.
     * Set by PairionHUD.focusPin() via LayerManager.currentBackground.activePinIndex.
     */
    property int activePinIndex: -1

    // ── Globe ─────────────────────────────────────────────────────

    HemisphereMap {
        id: worldMap
        anchors.fill: parent

        activePinIndex: root.activePinIndex

        // Server-driven map focus via MapFocus / MapClear WebSocket messages.
        serverFocus: ConnectionState.mapFocusActive
                     ? ({ "lat":  ConnectionState.mapFocusLat,
                          "lon":  ConnectionState.mapFocusLon,
                          "zoom": ConnectionState.mapFocusZoom })
                     : null

        pins: [
            { lat:  32.7767, lon:  -96.7970, city: "Dallas",   headline: "Market Volatility"  },
            { lat:  51.5074, lon:   -0.1278, city: "London",   headline: "Climate Summit"      },
            { lat:  35.6762, lon:  139.6503, city: "Tokyo",    headline: "Space Agency Update" },
            { lat:  40.7128, lon:  -74.0060, city: "New York", headline: "Cyberattack Alert"   },
            { lat: -33.8688, lon:  151.2093, city: "Sydney",   headline: "Renewable Energy"    }
        ]
    }
}
