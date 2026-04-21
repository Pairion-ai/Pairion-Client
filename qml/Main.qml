import QtQuick
import QtQuick.Controls
import "Debug"
import "HUD"

/**
 * Root application window.
 *
 * Full-screen cinematic HUD is shown by default. F12 toggles between the HUD
 * and the debug panel. F11 toggles the HUD's built-in FPS counter. Escape
 * exits the application.
 */
ApplicationWindow {
    id: root
    visibility: Window.FullScreen
    visible: true
    title: "Pairion"
    color: "#0a0e1a"

    // true → show HUD, false → show DebugPanel
    property bool hudActive: true

    // ── HUD ───────────────────────────────────────────────────────────────────

    PairionHUD {
        id: hud
        anchors.fill: parent
        visible: root.hudActive
    }

    // ── Debug panel ───────────────────────────────────────────────────────────

    Rectangle {
        anchors.fill: parent
        color: "#1a1a2e"
        visible: !root.hudActive

        DebugPanel {
            anchors.fill: parent
            anchors.margins: 16
        }
    }

    // ── Keyboard shortcuts ────────────────────────────────────────────────────

    // F12 — toggle HUD ↔ Debug panel
    Shortcut {
        sequence:  "F12"
        onActivated: root.hudActive = !root.hudActive
    }

    // F11 — toggle FPS counter (only meaningful when HUD is visible)
    Shortcut {
        sequence:  "F11"
        onActivated: if (root.hudActive) hud.toggleFps()
    }

    // Escape — exit application
    Shortcut {
        sequence:  "Escape"
        onActivated: Qt.quit()
    }
}
