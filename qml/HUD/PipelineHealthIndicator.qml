/**
 * @file PipelineHealthIndicator.qml
 * @brief Small always-visible HUD component surfacing pipeline health to the user.
 *
 * During startup, shows a sequenced status: Connecting → Downloading models →
 * Initializing → Ready (fades out after 3 seconds). During operation, shows
 * amber or red indicators for degraded or failed pipeline states.
 *
 * Bound to ConnectionState.pipelineHealth (string property).
 */
import QtQuick
import Pairion

Item {
    id: root

    width: statusRow.width + 16
    height: 28

    // ── Health state → display mapping ───────────────────────────────────────

    readonly property string health: ConnectionState.pipelineHealth

    readonly property color dotColor: {
        switch (health) {
        case "ready":           return "#00ff88"
        case "connecting":      return "#00b4ff"
        case "models_loading":  return "#00b4ff"
        case "initializing":    return "#00b4ff"
        case "mic_offline":     return "#ffaa00"
        case "wake_failed":     return "#ffaa00"
        case "server_disconnected": return "#ffaa00"
        case "models_failed":   return "#ff4444"
        case "pipeline_error":  return "#ff4444"
        default:                return "#888888"
        }
    }

    readonly property string statusText: {
        switch (health) {
        case "ready":               return "READY"
        case "connecting":          return "CONNECTING..."
        case "models_loading":      return "DOWNLOADING MODELS..."
        case "initializing":        return "INITIALIZING..."
        case "mic_offline":         return "MIC OFFLINE"
        case "wake_failed":         return "WAKE DETECTOR FAILED"
        case "server_disconnected": return "SERVER DISCONNECTED"
        case "models_failed":       return "MODEL LOAD FAILED"
        case "pipeline_error":      return "PIPELINE ERROR"
        default:                    return ""
        }
    }

    // ── Fade-out when ready ───────────────────────────────────────────────────

    opacity: 1.0
    Behavior on opacity { NumberAnimation { duration: 800; easing.type: Easing.InOutQuad } }

    onHealthChanged: {
        if (health === "ready") {
            fadeOutTimer.restart()
        } else {
            fadeOutTimer.stop()
            opacity = 1.0
        }
    }

    Timer {
        id: fadeOutTimer
        interval: 3000
        repeat: false
        onTriggered: root.opacity = 0.0
    }

    // ── Background pill ───────────────────────────────────────────────────────

    Rectangle {
        anchors.fill: parent
        radius: 4
        color: "#0d1220"
        opacity: 0.85
    }

    // ── Status row: dot + label ───────────────────────────────────────────────

    Row {
        id: statusRow
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 8
        spacing: 6

        Rectangle {
            width: 6
            height: 6
            radius: 3
            color: root.dotColor
            anchors.verticalCenter: parent.verticalCenter

            SequentialAnimation on opacity {
                running: root.health !== "ready"
                loops: Animation.Infinite
                NumberAnimation { to: 0.3; duration: 800; easing.type: Easing.InOutSine }
                NumberAnimation { to: 1.0; duration: 800; easing.type: Easing.InOutSine }
            }
        }

        Text {
            text: root.statusText
            color: root.dotColor
            font.pixelSize: 9
            font.family: "Courier New"
            font.letterSpacing: 1.5
            font.capitalization: Font.AllUppercase
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
