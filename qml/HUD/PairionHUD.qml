/**
 * @file PairionHUD.qml
 * @brief Cinematic full-screen HUD: TopBar, world map, ring system, dashboard panels, transcript.
 */
import QtQuick
import Pairion

Item {
    id: root

    /**
     * @brief Toggles the FPS counter overlay.
     */
    function toggleFps() { fpsCounter.visible = !fpsCounter.visible }

    /**
     * @brief Pan + zoom the world map to the pin at the given index.
     * Pass -1 to zoom out and resume auto-scroll.
     * Called by Main.qml test shortcuts today; wired to ConnectionState when
     * Alfred news narration is implemented.
     */
    function focusPin(index) { worldMap.activePinIndex = index }

    // ── Background ────────────────────────────────────────────────────────────

    Rectangle {
        anchors.fill: parent
        color: "#070c18"
    }

    // ── Context-driven background (space, etc.) ───────────────────────────────
    // Sits below the globe layer. When context is "earth" this layer is fully
    // transparent so the globe dominates. For other contexts it fades in while
    // the globe fades out.

    ContextBackground {
        id: contextBg
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: topBar.bottom
        anchors.bottom: dashboardPanels.top
        anchors.bottomMargin: 4
        context: ConnectionState.backgroundContext
    }

    // ── World map (full background) ───────────────────────────────────────────

    HemisphereMap {
        id: worldMap
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: topBar.bottom
        anchors.bottom: dashboardPanels.top
        anchors.bottomMargin: 4

        opacity: ConnectionState.backgroundContext === "earth" ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: 800; easing.type: Easing.InOutQuad } }

        // Server-driven map focus: Alfred focuses the globe via the MapFocus WebSocket message.
        // Setting serverFocus to null (MapClear) resumes auto-scroll.
        serverFocus: ConnectionState.mapFocusActive
                     ? ({ "lat":  ConnectionState.mapFocusLat,
                          "lon":  ConnectionState.mapFocusLon,
                          "zoom": ConnectionState.mapFocusZoom })
                     : null

        pins: [
            { lat: 32.7767, lon: -96.7970, city: "Dallas",    headline: "Market Volatility" },
            { lat: 51.5074, lon:  -0.1278, city: "London",    headline: "Climate Summit" },
            { lat: 35.6762, lon: 139.6503, city: "Tokyo",     headline: "Space Agency Update" },
            { lat: 40.7128, lon: -74.0060, city: "New York",  headline: "Cyberattack Alert" },
            { lat: -33.868, lon: 151.2093, city: "Sydney",    headline: "Renewable Energy" }
        ]
    }

    // ── Ring system — visible only while Alfred is speaking ──────────────────

    RingSystem {
        id: rings
        hudState: ConnectionState.agentState

        anchors.left:   parent.left
        anchors.right:  parent.right
        anchors.top:    topBar.bottom
        anchors.bottom: dashboardPanels.top

        opacity: ConnectionState.agentState === "speaking" ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: 600; easing.type: Easing.InOutQuad } }
    }

    // ── Top bar ───────────────────────────────────────────────────────────────

    TopBar {
        id: topBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 48
    }

    // ── Dashboard panels ──────────────────────────────────────────────────────

    DashboardPanels {
        id: dashboardPanels
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: transcriptStrip.top
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.bottomMargin: 4
        height: 180
    }

    // ── Transcript strip ──────────────────────────────────────────────────────

    TranscriptStrip {
        id: transcriptStrip
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 52
    }

    // ── FPS counter (hidden by default) ──────────────────────────────────────

    FpsCounter {
        id: fpsCounter
        visible: false
        anchors.top: topBar.bottom
        anchors.right: parent.right
        anchors.topMargin: 8
        anchors.rightMargin: 12
    }
}
