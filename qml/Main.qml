import QtQuick
import QtQuick.Controls
import "Debug"
import "HUD"
import Pairion

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

    property bool hudActive: true

    PairionHUD {
        id: hud
        anchors.fill: parent
        visible: root.hudActive
    }

    Rectangle {
        anchors.fill: parent
        color: "#1a1a2e"
        visible: !root.hudActive

        DebugPanel {
            anchors.fill: parent
            anchors.margins: 16
        }
    }

    Shortcut {
        sequence: "F12"
        onActivated: root.hudActive = !root.hudActive
    }

    Shortcut {
        sequence: "F11"
        onActivated: if (root.hudActive) hud.toggleFps()
    }

    Shortcut {
        sequence: "Escape"
        onActivated: Qt.quit()
    }

    // ── News-pin focus shortcuts (test / demo) ────────────────────────────────
    // Keys 1–5 pan + zoom to each news pin. Key 0 clears focus and resumes
    // globe auto-scroll. These shortcuts will be replaced by ConnectionState
    // bindings when Jarvis news narration is wired.

    Shortcut { sequence: "0"; onActivated: if (root.hudActive) hud.focusPin(-1) }
    Shortcut { sequence: "1"; onActivated: if (root.hudActive) hud.focusPin(0) }
    Shortcut { sequence: "2"; onActivated: if (root.hudActive) hud.focusPin(1) }
    Shortcut { sequence: "3"; onActivated: if (root.hudActive) hud.focusPin(2) }
    Shortcut { sequence: "4"; onActivated: if (root.hudActive) hud.focusPin(3) }
    Shortcut { sequence: "5"; onActivated: if (root.hudActive) hud.focusPin(4) }

    // ── Scene shortcuts (test / demo) ────────────────────────────────────────────
    // "B" cycles through the built-in scenes: globe → space → globe …
    // In production, the server sends SceneChange messages to drive scene transitions.

    readonly property var sceneIds: ["globe", "space"]

    Shortcut {
        sequence: "B"
        onActivated: {
            if (!root.hudActive) return
            var ids     = root.sceneIds
            var current = ConnectionState.activeSceneId
            var next    = ids[(ids.indexOf(current) + 1) % ids.length]
            ConnectionState.setActiveSceneId(next)
        }
    }
}
