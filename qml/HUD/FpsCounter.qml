/**
 * @file FpsCounter.qml
 * @brief Hidden FPS counter toggled by F11.
 *
 * Samples frame timestamps via a 250 ms polling timer and displays a rolling
 * average FPS in the top-right corner. Invisible by default; toggle with F11.
 * Positioned by the parent (PairionHUD) so it can be anchored inside the
 * full-screen window without extra layout items.
 */

import QtQuick

Item {
    id: root

    /**
     * @brief Whether the FPS counter is currently visible.
     * Toggled externally via the F11 key handler in PairionHUD.
     */
    property bool shown: false

    implicitWidth:  100
    implicitHeight: 28
    visible: shown

    // ── Frame timing ─────────────────────────────────────────────────────────

    property int    frameCount:  0
    property real   fps:         0.0
    property real   lastMs:      0.0

    // Increment frameCount every animation frame while the counter is visible.
    // FrameAnimation is available in Qt 6.4+.
    FrameAnimation {
        id: frameAnim
        running: root.shown
        onTriggered: root.frameCount++
    }

    // Sample the accumulated frameCount every 250 ms and compute average FPS.
    Timer {
        id: sampleTimer
        interval: 250
        repeat: true
        running: root.shown
        onTriggered: {
            root.fps        = root.frameCount * 4.0   // frames / 0.25 s → fps
            root.frameCount = 0
        }
    }

    // ── Display ───────────────────────────────────────────────────────────────

    Rectangle {
        anchors.fill: parent
        color:  "#88000000"
        radius: 4

        Text {
            anchors.centerIn: parent
            text:  root.fps.toFixed(0) + " fps"
            color: root.fps >= 55 ? "#00ff88"
                 : root.fps >= 40 ? "#ffcc00"
                 :                  "#ff4444"
            font.pixelSize: 13
            font.family:    "Courier New"
            font.bold:      true
        }
    }
}
