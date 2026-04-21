/**
 * @file PairionHUD.qml
 * @brief Full-screen cinematic HUD — rings, layout grid, hemisphere maps.
 *
 * Composites three layers:
 *   1. Dark navy background (#0a0e1a)
 *   2. RingSystem — five concentric animated arcs, centered, state-driven
 *   3. HudLayout — all dashboard panels (hemisphere maps, bottom panels,
 *                  transcript strip) positioned over the ring layer
 *
 * HUD state derivation (priority order):
 *   "error"     — ConnectionState.status == 0 (Disconnected)
 *   "listening" — voiceState is awaiting_wake / streaming / ending_speech
 *   "thinking"  — agentState == "thinking"
 *   "speaking"  — agentState == "speaking"
 *   "idle"      — all other conditions
 */

import QtQuick
import Pairion

Item {
    id: root

    // ── HUD state derivation ─────────────────────────────────────────────────

    /**
     * @brief Derived HUD state string passed to RingSystem and HudLayout.
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

    // ── Layer 1: Background ───────────────────────────────────────────────────

    Rectangle {
        anchors.fill: parent
        color: "#0a0e1a"
    }

    // ── Layer 2: Ring system — centered ───────────────────────────────────────

    RingSystem {
        id: rings
        anchors.centerIn: parent
        width:  Math.min(parent.width, parent.height)
        height: Math.min(parent.width, parent.height)
        hudState: root.hudState
    }

    // ── Layer 3: Dashboard panel layout ───────────────────────────────────────

    HudLayout {
        id: layout
        anchors.fill: parent
        hudState: root.hudState
    }

    // ── FPS counter — top-right, hidden by default ────────────────────────────

    FpsCounter {
        id: fpsCounter
        anchors {
            top:     parent.top
            right:   parent.right
            margins: 16
        }
        shown: false
    }

    /**
     * @brief Toggle the FPS counter visibility.
     * Called by the F11 Shortcut in Main.qml.
     */
    function toggleFps() {
        fpsCounter.shown = !fpsCounter.shown
    }
}
