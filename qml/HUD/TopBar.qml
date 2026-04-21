/**
 * @file TopBar.qml
 * @brief Full-width top bar showing world city clocks.
 *
 * Displays live clocks for Dallas (local), London (UTC+1), and Tokyo (UTC+9),
 * updated every second. Styled with a dark navy background and cyan text.
 */

import QtQuick

Item {
    id: root

    // ── Background ────────────────────────────────────────────────────────────

    Rectangle {
        anchors.fill: parent
        color: "#0d1220"
        opacity: 0.85
    }

    // ── 1-second clock tick ───────────────────────────────────────────────────

    property var now: new Date()

    Timer {
        interval: 1000
        repeat: true
        running: true
        onTriggered: root.now = new Date()
    }

    // ── City clocks ───────────────────────────────────────────────────────────

    Row {
        anchors.centerIn: parent
        spacing: 80

        Repeater {
            model: [
                { city: "DALLAS",  offsetH:  0, offsetM:  0 },   // local system time
                { city: "LONDON",  offsetH:  1, offsetM:  0 },   // UTC+1
                { city: "TOKYO",   offsetH:  9, offsetM:  0 }    // UTC+9
            ]

            delegate: Column {
                id: clockCol
                required property var modelData
                spacing: 2

                readonly property string timeStr: {
                    var d = root.now
                    var utcMs = d.getTime() + d.getTimezoneOffset() * 60000
                    var localMs = utcMs + (modelData.offsetH * 3600 + modelData.offsetM * 60) * 1000
                    // For DALLAS use system local time directly
                    var target = modelData.city === "DALLAS" ? d : new Date(localMs)
                    var h = target.getHours()
                    var m = target.getMinutes()
                    var s = target.getSeconds()
                    var ampm = h >= 12 ? "PM" : "AM"
                    h = h % 12; if (h === 0) h = 12
                    return (h < 10 ? "0" + h : h) + ":" +
                           (m < 10 ? "0" + m : m) + ":" +
                           (s < 10 ? "0" + s : s) + " " + ampm
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: clockCol.modelData.city
                    color: "#00b4ff"
                    opacity: 0.8
                    font.pixelSize: 10
                    font.letterSpacing: 2
                    font.family: "Courier New"
                    font.capitalization: Font.AllUppercase
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: clockCol.timeStr
                    color: "#ffffff"
                    font.pixelSize: 14
                    font.family: "Courier New"
                }
            }
        }
    }

    // ── Bottom divider ────────────────────────────────────────────────────────

    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: 1
        color: "#00b4ff"
        opacity: 0.25
    }
}
