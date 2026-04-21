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
     * Jarvis news narration is implemented.
     */
    function focusPin(index) { worldMap.activePinIndex = index }

    // ── Background ────────────────────────────────────────────────────────────

    Rectangle {
        anchors.fill: parent
        color: "#070c18"
    }

    // ── World map (full background) ───────────────────────────────────────────

    HemisphereMap {
        id: worldMap
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: topBar.bottom
        anchors.bottom: dashboardPanels.top
        anchors.bottomMargin: 4

        pins: [
            { lat: 32.7767, lon: -96.7970, city: "Dallas",    headline: "Market Volatility" },
            { lat: 51.5074, lon:  -0.1278, city: "London",    headline: "Climate Summit" },
            { lat: 35.6762, lon: 139.6503, city: "Tokyo",     headline: "Space Agency Update" },
            { lat: 40.7128, lon: -74.0060, city: "New York",  headline: "Cyberattack Alert" },
            { lat: -33.868, lon: 151.2093, city: "Sydney",    headline: "Renewable Energy" }
        ]
    }

    // ── Ring system (centered over the Atlantic) ──────────────────────────────

    RingSystem {
        id: rings
        hudState: ConnectionState.hudState

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.horizontalCenterOffset: -(parent.width * 0.18)
        anchors.top: topBar.bottom
        anchors.bottom: dashboardPanels.top
        width: parent.width * 0.7
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
