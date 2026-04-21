/**
 * @file RingSystem.qml
 * @brief Five concentric animated rings that respond to agent and voice state.
 *
 * Renders five independently-rotating arcs using Qt Quick Shapes. Ring color,
 * opacity, rotation speed, and arc span are all driven by the hudState property.
 * Supported states: "idle", "listening", "thinking", "speaking", "error".
 * All geometry is relative to parent size for resolution independence.
 */

import QtQuick
import QtQuick.Shapes
import QtQuick.Effects

Item {
    id: root

    // ── Public API ──────────────────────────────────────────────────────────

    /**
     * @brief Active HUD state: "idle" | "listening" | "thinking" | "speaking" | "error".
     * Transitions animate over 600 ms with InOutQuad easing.
     */
    property string hudState: "idle"

    // ── State-driven animated properties ────────────────────────────────────

    property color ringColor:     "#00b4ff"
    property real  opacityScale:  0.45
    property real  speedScale:    0.4
    property real  arcSpanScale:  0.75
    property real  pulseAmplitude: 0.0

    // ── State machine ────────────────────────────────────────────────────────

    states: [
        State {
            name: "idle"
            PropertyChanges {
                target: root
                ringColor:      "#00b4ff"
                opacityScale:   0.45
                speedScale:     0.4
                arcSpanScale:   0.75
                pulseAmplitude: 0.0
            }
        },
        State {
            name: "listening"
            PropertyChanges {
                target: root
                ringColor:      "#00b4ff"
                opacityScale:   1.0
                speedScale:     1.0
                arcSpanScale:   1.0
                pulseAmplitude: 0.08
            }
        },
        State {
            name: "thinking"
            PropertyChanges {
                target: root
                ringColor:      "#00b4ff"
                opacityScale:   0.85
                speedScale:     2.2
                arcSpanScale:   0.85
                pulseAmplitude: 0.0
            }
        },
        State {
            name: "speaking"
            PropertyChanges {
                target: root
                ringColor:      "#00b4ff"
                opacityScale:   1.0
                speedScale:     1.5
                arcSpanScale:   1.0
                pulseAmplitude: 0.12
            }
        },
        State {
            name: "error"
            PropertyChanges {
                target: root
                ringColor:      "#ff3c3c"
                opacityScale:   0.6
                speedScale:     0.6
                arcSpanScale:   0.6
                pulseAmplitude: 0.0
            }
        }
    ]

    state: hudState

    transitions: [
        Transition {
            ColorAnimation  { property: "ringColor";  duration: 600; easing.type: Easing.InOutQuad }
            NumberAnimation {
                properties: "opacityScale,speedScale,arcSpanScale,pulseAmplitude"
                duration: 600
                easing.type: Easing.InOutQuad
            }
        }
    ]

    // ── Pulse oscillator ─────────────────────────────────────────────────────

    property real pulsePhase: 0.0
    NumberAnimation on pulsePhase {
        from: 0.0; to: 6.28318   // 2π
        duration: 1800
        loops: Animation.Infinite
        running: true
    }

    // ── Per-ring configuration ───────────────────────────────────────────────

    // radiusFraction: fraction of half the shorter viewport dimension
    // baseOpacity: opacity at opacityScale=1
    // dir: +1 clockwise, -1 counter-clockwise
    // arcSpan: degrees of arc when arcSpanScale=1
    // strokeWidth: crisp arc line width in pixels
    // basePeriod: rotation period in ms at speedScale=1
    readonly property var ringConfigs: [
        { radiusFraction: 0.18, baseOpacity: 0.90, dir:  1, arcSpan: 300, strokeWidth: 3, basePeriod: 7000 },
        { radiusFraction: 0.26, baseOpacity: 0.70, dir: -1, arcSpan: 260, strokeWidth: 2, basePeriod: 9500 },
        { radiusFraction: 0.34, baseOpacity: 0.55, dir:  1, arcSpan: 220, strokeWidth: 2, basePeriod: 11000 },
        { radiusFraction: 0.42, baseOpacity: 0.38, dir: -1, arcSpan: 200, strokeWidth: 1, basePeriod: 14000 },
        { radiusFraction: 0.50, baseOpacity: 0.22, dir:  1, arcSpan: 180, strokeWidth: 1, basePeriod: 18000 }
    ]

    // ── Rings ─────────────────────────────────────────────────────────────────

    Repeater {
        model: root.ringConfigs.length

        delegate: Item {
            id: ringItem

            required property int index

            readonly property var cfg: root.ringConfigs[index]
            readonly property real radius: Math.min(root.width, root.height) * 0.5 * cfg.radiusFraction

            // Pulse contribution — each ring offset in phase for a ripple effect
            readonly property real pulseOffset: Math.sin(root.pulsePhase + index * 0.7) * root.pulseAmplitude
            readonly property real effectiveOpacity: Math.max(0.0, Math.min(1.0,
                cfg.baseOpacity * root.opacityScale + pulseOffset))
            readonly property real effectiveArcSpan: cfg.arcSpan * root.arcSpanScale

            // Rotation angle driven by timer-based increment (smooth speed changes)
            property real rotationAngle: 0.0

            NumberAnimation on rotationAngle {
                id: rotAnim
                from: 0; to: cfg.dir > 0 ? 360 : -360
                // Clamp to avoid zero/negative which would stall or reverse unexpectedly
                duration: Math.max(500, cfg.basePeriod / root.speedScale)
                loops: Animation.Infinite
                running: true
            }

            width: root.width
            height: root.height
            anchors.centerIn: root

            // ── Glow (blurred underscan) ─────────────────────────────────────

            Shape {
                id: glowShape
                width: parent.width
                height: parent.height
                anchors.centerIn: parent
                opacity: ringItem.effectiveOpacity * 0.55
                rotation: ringItem.rotationAngle

                layer.enabled: true
                layer.effect: MultiEffect {
                    blurEnabled: true
                    blur: 1.0
                    blurMax: 32
                }

                ShapePath {
                    strokeColor: root.ringColor
                    fillColor:   "transparent"
                    strokeWidth: cfg.strokeWidth * 7
                    capStyle: ShapePath.RoundCap

                    PathAngleArc {
                        centerX:    glowShape.width  * 0.5
                        centerY:    glowShape.height * 0.5
                        radiusX:    ringItem.radius
                        radiusY:    ringItem.radius
                        startAngle: -90
                        sweepAngle: ringItem.effectiveArcSpan
                    }
                }
            }

            // ── Crisp arc ────────────────────────────────────────────────────

            Shape {
                id: arcShape
                width: parent.width
                height: parent.height
                anchors.centerIn: parent
                opacity: ringItem.effectiveOpacity
                rotation: ringItem.rotationAngle

                ShapePath {
                    strokeColor: root.ringColor
                    fillColor:   "transparent"
                    strokeWidth: cfg.strokeWidth
                    capStyle: ShapePath.RoundCap

                    PathAngleArc {
                        centerX:    arcShape.width  * 0.5
                        centerY:    arcShape.height * 0.5
                        radiusX:    ringItem.radius
                        radiusY:    ringItem.radius
                        startAngle: -90
                        sweepAngle: ringItem.effectiveArcSpan
                    }
                }
            }
        }
    }
}
