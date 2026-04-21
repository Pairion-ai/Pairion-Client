/**
 * @file ContextBackground.qml
 * @brief Context-driven background layer for non-Earth scenes.
 *
 * Renders a different background depending on the context property.
 * Supported contexts:
 *   "earth"  — fully transparent; the HemisphereMap globe shows through.
 *   "space"  — animated star-field with bright accent stars and a nebula haze.
 *
 * All context layers crossfade over 800 ms on context change so transitions
 * are seamless behind the ring system and other HUD chrome.
 *
 * To add a new context: create an Item inside root, set its opacity binding
 * to `root.context === "<name>" ? 1.0 : 0.0` with a Behavior, and give it
 * a unique id.
 *
 * Star positions use a deterministic pseudo-random sequence keyed on the
 * delegate index so they are stable across window resizes.
 */
import QtQuick

Item {
    id: root

    /**
     * @brief Active background context.
     * "earth" renders nothing (globe shows through).
     * "space" renders an animated star-field.
     */
    property string context: "earth"

    // ── Space scene ───────────────────────────────────────────────────────────

    Item {
        id: spaceScene
        anchors.fill: parent
        opacity: root.context === "space" ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: 800; easing.type: Easing.InOutQuad } }

        // Deep-space gradient: near-black with a cobalt tint
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "#01020c" }
                GradientStop { position: 0.5; color: "#010511" }
                GradientStop { position: 1.0; color: "#01020c" }
            }
        }

        // Nebula haze — soft blue-purple glow, left-centre
        Rectangle {
            x: root.width * 0.04
            y: root.height * 0.18
            width:  root.width  * 0.48
            height: root.height * 0.64
            radius: height * 0.5
            color: "#06053c"
            opacity: 0.55
        }

        // Faint secondary nebula haze — upper-right
        Rectangle {
            x: root.width * 0.58
            y: root.height * 0.05
            width:  root.width  * 0.35
            height: root.height * 0.40
            radius: height * 0.5
            color: "#0a0320"
            opacity: 0.40
        }

        // ── Background stars (small, twinkling) ───────────────────────────────

        Repeater {
            model: 180

            delegate: Rectangle {
                required property int index

                // Deterministic pseudo-random positions: avoids Math.random()
                // re-evaluation on width/height changes.
                readonly property real rndX:   (index * 2654435761 % 99991) / 99991.0
                readonly property real rndY:   (index * 1013904223 % 99991) / 99991.0
                readonly property real rndSz:  0.6 + (index * 6364136 % 10) / 10.0 * 1.4
                readonly property real rndBase: 0.12 + (index % 11) * 0.045

                x: rndX * root.width  - rndSz * 0.5
                y: rndY * root.height - rndSz * 0.5
                width:  rndSz
                height: rndSz
                radius: rndSz * 0.5

                // Slightly warm/cool whites for variation
                color: Qt.rgba(0.84 + (index % 5)  * 0.032,
                               0.87 + (index % 3)  * 0.043,
                               1.0, 1.0)

                SequentialAnimation on opacity {
                    loops: Animation.Infinite
                    running: spaceScene.opacity > 0.01
                    PauseAnimation  { duration: (index * 307) % 6500 }
                    NumberAnimation { to: rndBase * 0.12; duration: 650 + (index * 179) % 1200; easing.type: Easing.InOutSine }
                    NumberAnimation { to: rndBase;        duration: 650 + (index * 211) % 1200; easing.type: Easing.InOutSine }
                }
            }
        }

        // ── Bright accent stars (larger, diffraction spikes) ─────────────────

        Repeater {
            model: 9

            delegate: Item {
                required property int index

                readonly property real bx: (index * 1234567891 % 99991) / 99991.0
                readonly property real by: (index * 987654321  % 99991) / 99991.0

                x: bx * root.width  - 6
                y: by * root.height - 6
                width:  12
                height: 12

                // Core dot
                Rectangle {
                    id: coreDot
                    anchors.centerIn: parent
                    width: 3; height: 3
                    radius: 1.5
                    color: "white"

                    SequentialAnimation on opacity {
                        loops: Animation.Infinite
                        running: spaceScene.opacity > 0.01
                        NumberAnimation { to: 0.35; duration: 1100 + index * 370; easing.type: Easing.InOutSine }
                        NumberAnimation { to: 0.95; duration: 1100 + index * 370; easing.type: Easing.InOutSine }
                    }
                }

                // Horizontal diffraction spike
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    x: -4
                    width: 20; height: 1
                    color: "white"
                    opacity: 0.22
                }

                // Vertical diffraction spike
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: -4
                    width: 1; height: 20
                    color: "white"
                    opacity: 0.22
                }
            }
        }
    }
}
