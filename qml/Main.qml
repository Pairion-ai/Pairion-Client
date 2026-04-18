import QtQuick
import QtQuick.Controls
import "Debug"

/**
 * Root application window. At M0, displays the debug panel.
 * In later milestones, switches between debug and HUD based on state.
 */
ApplicationWindow {
    id: root
    width: 480
    height: 640
    visible: true
    title: "Pairion — Debug"
    color: "#1a1a2e"

    DebugPanel {
        anchors.fill: parent
        anchors.margins: 16
    }
}
