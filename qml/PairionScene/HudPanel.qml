/**
 * @file HudPanel.qml
 * @brief Reusable dark panel with a cyan border for use inside scene plugins.
 *
 * Provides a standard Pairion panel chrome: dark navy background, single-pixel
 * cyan border, and optional header label with badge count.
 */
import QtQuick

Item {
    id: root

    /** Panel header label displayed in the top-left corner. */
    property string title: ""

    /** Badge count overlaid in amber if > 0. */
    property int badgeCount: 0

    // ── Background ────────────────────────────────────────────────

    Rectangle {
        anchors.fill: parent
        color:        "#0d1220"
        border.color: "#00b4ff"
        border.width: 1
        radius:       4
    }

    // ── Header label ──────────────────────────────────────────────

    Text {
        id: titleLabel
        anchors { left: parent.left; top: parent.top; leftMargin: 8; topMargin: 5 }
        text:    root.title
        color:   "#00b4ff"
        opacity: 0.7
        font.pixelSize:      9
        font.letterSpacing:  2
        font.family:         "Courier New"
        font.capitalization: Font.AllUppercase
        visible: root.title.length > 0
    }

    // ── Badge ─────────────────────────────────────────────────────

    Rectangle {
        id: badge
        anchors { right: parent.right; top: parent.top; rightMargin: 6; topMargin: 5 }
        width:   20; height: 13
        radius:  3
        color:   "#ff8800"
        visible: root.badgeCount > 0

        Text {
            anchors.centerIn: parent
            text:      root.badgeCount > 9 ? "9+" : root.badgeCount.toString()
            color:     "white"
            font.pixelSize: 8
            font.family:    "Courier New"
            font.bold:      true
        }
    }
}
