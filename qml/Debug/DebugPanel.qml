import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Pairion 1.0

/**
 * M0 debug panel showing connection state, session ID, server version,
 * reconnect attempts, and the last 10 log records.
 */
Item {
    id: debugPanel

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        // --- Header ---
        Text {
            text: "Pairion Debug Panel"
            font.pixelSize: 20
            font.bold: true
            color: "#e0e0e0"
            Layout.alignment: Qt.AlignHCenter
        }

        // --- Connection status with colored dot ---
        RowLayout {
            spacing: 8
            Layout.alignment: Qt.AlignHCenter

            Rectangle {
                id: statusDot
                width: 14
                height: 14
                radius: 7
                color: {
                    switch (ConnectionState.status) {
                    case 0: return "#ff4444"   // Disconnected
                    case 1: return "#ffaa00"   // Connecting
                    case 2: return "#44ff44"   // Connected
                    case 3: return "#ffaa00"   // Reconnecting
                    }
                    return "#888888"
                }
                Behavior on color { ColorAnimation { duration: 300 } }
            }

            Text {
                text: {
                    switch (ConnectionState.status) {
                    case 0: return "Disconnected"
                    case 1: return "Connecting"
                    case 2: return "Connected"
                    case 3: return "Reconnecting"
                    }
                    return "Unknown"
                }
                font.pixelSize: 16
                font.bold: true
                color: "#e0e0e0"
            }
        }

        // --- Session info grid ---
        GridLayout {
            columns: 2
            columnSpacing: 12
            rowSpacing: 6
            Layout.fillWidth: true

            Text { text: "Session ID:"; color: "#aaaaaa"; font.pixelSize: 13 }
            Text {
                text: ConnectionState.sessionId || "—"
                color: "#e0e0e0"
                font.pixelSize: 13
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            Text { text: "Server Version:"; color: "#aaaaaa"; font.pixelSize: 13 }
            Text {
                text: ConnectionState.serverVersion || "—"
                color: "#e0e0e0"
                font.pixelSize: 13
            }

            Text { text: "Reconnect Attempts:"; color: "#aaaaaa"; font.pixelSize: 13 }
            Text {
                text: ConnectionState.reconnectAttempts.toString()
                color: "#e0e0e0"
                font.pixelSize: 13
            }
        }

        // --- Separator ---
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#333355"
        }

        // --- Log header ---
        Text {
            text: "Recent Logs"
            font.pixelSize: 14
            font.bold: true
            color: "#aaaaaa"
        }

        // --- Scrolling log list ---
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: ConnectionState.recentLogs

            delegate: Text {
                required property string modelData
                required property int index
                width: ListView.view.width
                text: modelData
                color: "#cccccc"
                font.pixelSize: 11
                font.family: "monospace"
                wrapMode: Text.WrapAnywhere
                topPadding: 2
                bottomPadding: 2
            }

            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        }
    }
}
