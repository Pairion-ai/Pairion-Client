/**
 * @file DashboardPanel.qml
 * @brief Reusable HUD panel container with header label and content slot.
 *
 * Renders a dark translucent rectangle with a 1px cyan border and an uppercase
 * letter-spaced header. The default content area is empty — child items placed
 * inside fill the content region. Used by all bottom dashboard panels and the
 * hemisphere map frames.
 */

import QtQuick

Item {
    id: root

    /**
     * @brief Panel header label (rendered uppercase automatically).
     */
    property string title: ""

    /**
     * @brief Default property alias so child items land inside contentItem.
     */
    default property alias content: contentItem.data

    // ── Border frame ─────────────────────────────────────────────────────────

    Rectangle {
        id: frame
        anchors.fill: parent
        color:        "#0d1220"
        opacity:      0.40
        radius:       4
        border.color: "#00b4ff"
        border.width: 1
    }

    // Separate border rect at full opacity so the border isn't tinted by the
    // parent opacity — the fill is translucent, the border is independent.
    Rectangle {
        anchors.fill: parent
        color:        "transparent"
        radius:       4
        border.color: Qt.rgba(0, 0.706, 1.0, 0.20)
        border.width: 1
    }

    // ── Header ────────────────────────────────────────────────────────────────

    Text {
        id: headerText
        anchors {
            top:              parent.top
            left:             parent.left
            right:            parent.right
            topMargin:        6
            leftMargin:       8
        }
        text:                root.title
        color:               Qt.rgba(0, 0.706, 1.0, 0.60)
        font.pixelSize:      10
        font.letterSpacing:  2
        font.family:         "Courier New"
        font.capitalization: Font.AllUppercase
    }

    // ── Content area ─────────────────────────────────────────────────────────

    Item {
        id: contentItem
        anchors {
            top:         headerText.bottom
            left:        parent.left
            right:       parent.right
            bottom:      parent.bottom
            topMargin:   4
            leftMargin:  8
            rightMargin: 8
            bottomMargin: 8
        }
    }
}
