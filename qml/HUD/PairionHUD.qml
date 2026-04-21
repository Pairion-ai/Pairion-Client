/**
 * @file PairionHUD.qml
 * @brief Full-screen cinematic HUD with state-driven concentric ring system.
 *
 * PairionHUD is the primary UI surface for the Pairion Client. It occupies the
 * entire application window, renders five concentric animated rings via
 * RingSystem, and exposes an FPS counter (F11) for performance monitoring.
 *
 * State derivation:
 *   - "error"     — ConnectionState.status == 0 (Disconnected)
 *   - "listening" — ConnectionState.voiceState is "awaiting_wake" or "streaming"
 *   - "thinking"  — ConnectionState.agentState == "thinking"
 *   - "speaking"  — ConnectionState.agentState == "speaking"
 *   - "idle"      — all other conditions
 */

import QtQuick
import Pairion

Item {
    id: root

    // ── HUD state derivation ─────────────────────────────────────────────────

    /**
     * @brief Derived HUD state string passed to RingSystem.
     *
     * Priority: error > listening > thinking > speaking > idle.
     */
    readonly property string hudState: {
        if (ConnectionState.status === 0)
            return "error"
        const vs = ConnectionState.voiceState
        if (vs === "awaiting_wake" || vs === "streaming" || vs === "ending_speech")
            return "listening"
        const as = ConnectionState.agentState
        if (as === "thinking")
            return "thinking"
        if (as === "speaking")
            return "speaking"
        return "idle"
    }

    // ── Dark navy background ─────────────────────────────────────────────────

    Rectangle {
        anchors.fill: parent
        color: "#0a0e1a"
    }

    // ── Ring system — centered in viewport ───────────────────────────────────

    RingSystem {
        id: rings
        anchors.centerIn: parent
        // Use the full shorter dimension so rings are always circular
        width:  Math.min(parent.width, parent.height)
        height: Math.min(parent.width, parent.height)
        hudState: root.hudState
    }

    // ── Status label — subtle text below rings ────────────────────────────────

    Text {
        id: statusLabel
        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom: parent.bottom
            bottomMargin: 60
        }
        text: {
            switch (root.hudState) {
                case "error":     return "DISCONNECTED"
                case "listening": return "LISTENING"
                case "thinking":  return "THINKING"
                case "speaking":  return "SPEAKING"
                default:          return ""
            }
        }
        color:          "#00b4ff"
        opacity:        root.hudState === "idle" ? 0.0 : 0.55
        font.pixelSize: 13
        font.letterSpacing: 4
        font.family:    "Courier New"
        font.capitalization: Font.AllUppercase

        Behavior on opacity {
            NumberAnimation { duration: 400; easing.type: Easing.InOutQuad }
        }
    }

    // ── FPS counter — top-right, hidden by default ────────────────────────────

    FpsCounter {
        id: fpsCounter
        anchors {
            top:    parent.top
            right:  parent.right
            margins: 16
        }
        shown: false   // toggled by F11 in Main.qml key handler
    }

    /**
     * @brief Toggle the FPS counter visibility.
     * Called by the F11 KeyboardShortcut in Main.qml.
     */
    function toggleFps() {
        fpsCounter.shown = !fpsCounter.shown
    }
}
