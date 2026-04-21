/**
 * @file TranscriptStrip.qml
 * @brief Full-width transcript strip bound to ConnectionState.
 *
 * Shows the most recent final transcript and the in-progress partial
 * transcript below it. Styled with a dark translucent background and
 * cyan border top.
 */

import QtQuick
import Pairion

Item {
    id: root

    // ── Background ────────────────────────────────────────────────────────────

    Rectangle {
        anchors.fill: parent
        color: "#0d1220"
        opacity: 0.7
    }

    Rectangle {
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 1
        color: "#00b4ff"
        opacity: 0.18
    }

    // ── Header ────────────────────────────────────────────────────────────────

    Text {
        anchors { left: parent.left; top: parent.top; leftMargin: 10; topMargin: 6 }
        text: "TRANSCRIPT"
        color: "#00b4ff"
        opacity: 0.55
        font.pixelSize: 9
        font.letterSpacing: 2
        font.family: "Courier New"
        font.capitalization: Font.AllUppercase
    }

    // ── Content ───────────────────────────────────────────────────────────────

    Column {
        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter;
                  leftMargin: 10; rightMargin: 10; verticalCenterOffset: 5 }
        spacing: 2

        Text {
            width: parent.width
            text: ConnectionState.transcriptFinal
            color: "#e0e8f0"
            opacity: 0.9
            font.pixelSize: 11
            font.family: "Courier New"
            elide: Text.ElideRight
            visible: text.length > 0
        }

        Text {
            width: parent.width
            text: ConnectionState.transcriptPartial
            color: "#e0e8f0"
            opacity: 0.5
            font.pixelSize: 11
            font.family: "Courier New"
            font.italic: true
            elide: Text.ElideRight
            visible: text.length > 0
        }
    }
}
