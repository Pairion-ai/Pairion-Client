/**
 * @file GlobeScene.qml
 * @brief Globe scene plugin — wraps HemisphereMap with server-driven focus and news pins.
 *
 * This scene is the default (activeSceneId = "globe"). It renders the animated
 * day/night hemisphere map with five news-pin overlays and responds to
 * ConnectionState.mapFocusActive / mapFocusLat / mapFocusLon / mapFocusZoom
 * exactly as HemisphereMap previously did when hosted directly in PairionHUD.
 *
 * PairionHUD.focusPin() forwards pin-index changes through SceneManager.currentScene,
 * which updates the activePinIndex property exposed here.
 */
import QtQuick
import Pairion
import "../../HUD"

Item {
    id: root

    // ── SceneBase contract ────────────────────────────────────────

    /** Server-pushed model data keyed by modelId. */
    property var    sceneData:   ({})
    /** Parameters from the activating SceneChange command. */
    property var    sceneParams: ({})
    /** Current agent state string from SceneManager. */
    property string hudState:    "idle"

    /** @brief Request scene switch (forwarded to SceneManager when needed). */
    signal requestScene(string sceneId, var params)
    /** @brief Request TTS narration. */
    signal requestSpeak(string text)

    // ── Globe-specific API ────────────────────────────────────────

    /**
     * @brief Zero-based index of the pin to focus, or -1 to resume auto-scroll.
     * Set by PairionHUD.focusPin() via SceneManager.currentScene.activePinIndex.
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
