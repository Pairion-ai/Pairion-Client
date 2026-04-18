import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Pairion 1.0

/**
 * M0/M1 debug panel showing connection state, session info, voice pipeline state,
 * transcript, LLM response, agent state, and the last 10 log records.
 */
Item {
    id: debugPanel

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

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
                    case 0: return "#ff4444"
                    case 1: return "#ffaa00"
                    case 2: return "#44ff44"
                    case 3: return "#ffaa00"
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

        // --- Session + Voice info grid ---
        GridLayout {
            columns: 2
            columnSpacing: 12
            rowSpacing: 4
            Layout.fillWidth: true

            Text { text: "Session ID:"; color: "#aaaaaa"; font.pixelSize: 12 }
            Text {
                text: ConnectionState.sessionId || "\u2014"
                color: "#e0e0e0"; font.pixelSize: 12
                elide: Text.ElideRight; Layout.fillWidth: true
            }

            Text { text: "Server Version:"; color: "#aaaaaa"; font.pixelSize: 12 }
            Text { text: ConnectionState.serverVersion || "\u2014"; color: "#e0e0e0"; font.pixelSize: 12 }

            Text { text: "Reconnect Attempts:"; color: "#aaaaaa"; font.pixelSize: 12 }
            Text { text: ConnectionState.reconnectAttempts.toString(); color: "#e0e0e0"; font.pixelSize: 12 }

            Text { text: "Voice Pipeline:"; color: "#aaaaaa"; font.pixelSize: 12 }
            Text {
                text: ConnectionState.voiceState || "idle"
                color: {
                    switch (ConnectionState.voiceState) {
                    case "awaiting_wake": return "#5599ff"
                    case "streaming": return "#44ff44"
                    case "ending_speech": return "#ffaa00"
                    default: return "#888888"
                    }
                }
                font.pixelSize: 12; font.bold: true
            }

            Text { text: "Agent State:"; color: "#aaaaaa"; font.pixelSize: 12 }
            Text {
                text: ConnectionState.agentState || "\u2014"
                color: "#e0e0e0"; font.pixelSize: 12
            }
        }

        // --- Separator ---
        Rectangle { Layout.fillWidth: true; height: 1; color: "#333355" }

        // --- Transcript section ---
        Text { text: "Transcript"; font.pixelSize: 13; font.bold: true; color: "#aaaaaa" }
        Text {
            text: ConnectionState.transcriptPartial
                  ? "[partial] " + ConnectionState.transcriptPartial
                  : ""
            color: "#88aacc"; font.pixelSize: 11; font.italic: true
            visible: ConnectionState.transcriptPartial !== ""
            Layout.fillWidth: true; wrapMode: Text.WrapAnywhere
        }
        Text {
            text: ConnectionState.transcriptFinal
                  ? "[final] " + ConnectionState.transcriptFinal
                  : ""
            color: "#ccddee"; font.pixelSize: 12
            visible: ConnectionState.transcriptFinal !== ""
            Layout.fillWidth: true; wrapMode: Text.WrapAnywhere
        }

        // --- LLM Response section ---
        Text { text: "LLM Response"; font.pixelSize: 13; font.bold: true; color: "#aaaaaa" }
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 80
            clip: true
            Text {
                text: ConnectionState.llmResponse || "\u2014"
                color: "#e0e0e0"; font.pixelSize: 11; font.family: "monospace"
                width: parent.width; wrapMode: Text.WrapAnywhere
            }
        }

        // --- Separator ---
        Rectangle { Layout.fillWidth: true; height: 1; color: "#333355" }

        // --- Log header ---
        Text { text: "Recent Logs"; font.pixelSize: 13; font.bold: true; color: "#aaaaaa" }

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
                font.pixelSize: 10
                font.family: "monospace"
                wrapMode: Text.WrapAnywhere
                topPadding: 1; bottomPadding: 1
            }

            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
        }
    }
}
