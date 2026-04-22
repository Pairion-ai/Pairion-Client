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
     * @brief Pan + zoom the globe to the pin at the given index.
     * Pass -1 to zoom out and resume auto-scroll.
     * Forwards to GlobeScene.activePinIndex via SceneManager.currentScene so the
     * API remains stable regardless of which scene is currently active.
     */
    function focusPin(index) {
        var scene = sceneManager.currentScene
        if (scene && "activePinIndex" in scene)
            scene.activePinIndex = index
    }

    // ── Background ────────────────────────────────────────────────────────────

    Rectangle {
        anchors.fill: parent
        color: "#070c18"
    }

    // ── Scene manager — replaces ContextBackground + HemisphereMap ──────────────
    // Loads and transitions between scene plugins driven by ConnectionState.activeSceneId.
    // GlobeScene ("globe"), SpaceScene ("space"), and DashboardScene ("dashboard") are the
    // three built-in plugins. Crossfade and instant transitions are supported.

    SceneManager {
        id: sceneManager
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: topBar.bottom
        anchors.bottom: dashboardPanels.top
        anchors.bottomMargin: 4
    }

    // ── Ring system — visible only while Jarvis is speaking ──────────────────

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
