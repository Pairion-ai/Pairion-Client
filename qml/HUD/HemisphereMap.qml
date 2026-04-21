/**
 * @file HemisphereMap.qml
 * @brief Full world map using NASA Black Marble image with seamless bidirectional wrapping,
 *        pin focus panning, and zoom.
 *
 * Three copies of the equirectangular map are laid out side-by-side inside a clipped
 * container. A Timer shifts mapOffset each frame for continuous globe rotation.
 *
 * When activePinIndex is set to a valid pin index the auto-scroll pauses, the map
 * animates horizontally so that pin lands at screen centre, and the map zooms in.
 * Setting activePinIndex back to -1 zooms out and resumes auto-scroll.
 *
 * Shortest-path wrapping: the pan animation always takes the shorter arc around the
 * globe (left or right) so the map never spins more than half a revolution to reach
 * any pin.
 *
 * The image uses a 30°W longitude shift so the Atlantic Ocean sits at x=50%.
 */

import QtQuick

Item {
    id: root

    // ── Public API ────────────────────────────────────────────────────────────

    /** @brief News pin list. Each element: { lat, lon, city, headline } */
    property var pins: []

    /**
     * @brief Index into pins[] that Jarvis is currently narrating. -1 = none.
     * Set this from the outside (ConnectionState binding or test shortcut) to
     * trigger pan + zoom. Reset to -1 to zoom out and resume auto-scroll.
     */
    property int activePinIndex: -1

    /**
     * @brief Auto-scroll speed in pixels per frame.
     * Positive = scroll left (eastward). Negative = scroll right (westward).
     */
    property real scrollSpeed: 0.4

    // ── Internal state ────────────────────────────────────────────────────────

    readonly property real lonShift: 30.0

    property real mapOffset: 0      // in (0, 2*width); initialised after layout
    property real zoomScale: 1.0

    // No clip: the map spans full screen width so side-neighbour images sit
    // outside the window bounds and are invisible without clipping. Removing
    // clip allows mapContainer's scale transform to expand visibly when a pin
    // is focused (clip would have cancelled the zoom back to original bounds).

    // Initialise once we know our width.
    onWidthChanged: {
        if (mapOffset === 0 && root.width > 0)
            mapOffset = root.width
    }

    // ── Pin focus logic ───────────────────────────────────────────────────────

    onActivePinIndexChanged: {
        focusSequence.stop()
        zoomOutAnim.stop()

        if (activePinIndex >= 0 && activePinIndex < pins.length) {
            // Compute target mapOffset that places this pin at screen centre.
            //   centre-image left edge = (width - mapOffset)
            //   pin screen x = (width - mapOffset) + basePinX = width / 2
            //   → mapOffset = width/2 + basePinX
            var pin      = pins[activePinIndex]
            var shifted  = pin.lon + lonShift
            var normLon  = ((shifted % 360) + 360) % 360
            var basePinX = (normLon / 360) * root.width
            var target   = root.width / 2 + basePinX

            // Shortest-path wrap (never spin more than half a revolution).
            var diff = target - root.mapOffset
            if (Math.abs(diff - root.width) < Math.abs(diff)) diff -= root.width
            if (Math.abs(diff + root.width) < Math.abs(diff)) diff += root.width

            seqPan.to = root.mapOffset + diff
            focusSequence.start()
        } else {
            // Clear: zoom out and resume auto-scroll.
            zoomOutAnim.start()
        }
    }

    // ── Animations ────────────────────────────────────────────────────────────

    /**
     * @brief Three-phase focus sequence: zoom out → (pan + delayed zoom-in).
     *
     * The pan and zoom-in run in parallel: the zoom-in starts 650 ms into the
     * 1300 ms pan (halfway), so the map is still travelling when it begins to
     * enlarge. This blends arrival and zoom into a single fluid motion rather
     * than two distinct steps.
     */
    SequentialAnimation {
        id: focusSequence

        // Phase 1 — pull back to show full globe context
        NumberAnimation {
            target: root; property: "zoomScale"
            to: 1.0; duration: 450; easing.type: Easing.InOutQuad
        }

        // Phase 2 — pan and zoom-in overlap
        ParallelAnimation {

            // Pan to centre the pin (target set in onActivePinIndexChanged)
            SequentialAnimation {
                NumberAnimation {
                    id: seqPan
                    target: root; property: "mapOffset"
                    duration: 1300; easing.type: Easing.InOutCubic
                }
                // Re-normalise mapOffset into (0, 2*width) after the pan.
                ScriptAction {
                    script: {
                        var w = root.width
                        if (w > 0) {
                            var o = root.mapOffset
                            while (o >= 2 * w) o -= w
                            while (o <= 0)     o += w
                            root.mapOffset = o
                        }
                    }
                }
            }

            // Zoom-in begins halfway through the pan (650 ms delay)
            SequentialAnimation {
                PauseAnimation { duration: 650 }
                NumberAnimation {
                    target: root; property: "zoomScale"
                    to: 1.6; duration: 850; easing.type: Easing.OutCubic
                }
            }
        }
    }

    /** @brief Zoom out and resume globe rotation when focus is cleared. */
    NumberAnimation {
        id: zoomOutAnim
        target: root; property: "zoomScale"
        to: 1.0; duration: 600; easing.type: Easing.InOutQuad
    }

    // ── Auto-scroll engine ────────────────────────────────────────────────────

    Timer {
        interval: 16
        repeat:   true
        running:  root.activePinIndex < 0 && root.width > 0 && !focusSequence.running

        onTriggered: {
            var w = root.width
            if (w <= 0) return
            var next = root.mapOffset + root.scrollSpeed
            if (next >= 2 * w) next -= w
            if (next <= 0)     next += w
            root.mapOffset = next
        }
    }

    // ── Map container — zoom is applied here ──────────────────────────────────

    Item {
        id: mapContainer
        width:  root.width
        height: root.height
        scale:  root.zoomScale
        transformOrigin: Item.Center   // expand toward all edges equally

        // Left neighbour — visible when scrolling right / zooming into western pins
        Image {
            x: -root.mapOffset
            width: root.width; height: root.height
            source: "qrc:/resources/maps/world_map.png"
            fillMode: Image.Stretch; smooth: true; antialiasing: true
        }

        // Centre copy — fills the screen at mapOffset == width
        Image {
            x: root.width - root.mapOffset
            width: root.width; height: root.height
            source: "qrc:/resources/maps/world_map.png"
            fillMode: Image.Stretch; smooth: true; antialiasing: true
        }

        // Right neighbour — visible when scrolling left / zooming into eastern pins
        Image {
            x: 2 * root.width - root.mapOffset
            width: root.width; height: root.height
            source: "qrc:/resources/maps/world_map.png"
            fillMode: Image.Stretch; smooth: true; antialiasing: true
        }

        // ── News pins (inside container so they scale with the zoom) ──────────

        Repeater {
            model: root.pins

            delegate: Item {
                id: pinItem
                required property var modelData
                required property int index

                readonly property bool isActive: index === root.activePinIndex

                readonly property real shiftedLon:   modelData.lon + root.lonShift
                readonly property real normalizedLon: ((shiftedLon % 360) + 360) % 360
                readonly property real basePinX:      (normalizedLon / 360) * root.width

                // Horizontal position tracks the scroll offset with wrapping.
                readonly property real pinX: ((root.width + basePinX - root.mapOffset) % root.width + root.width) % root.width
                readonly property real pinY: (1.0 - (modelData.lat + 90) / 180) * root.height

                x: pinX - 7
                y: pinY - 7
                width: 14; height: 14

                // Pulse animation — active pin beats faster
                property real pulseOpacity: 0.8
                SequentialAnimation on pulseOpacity {
                    loops: Animation.Infinite
                    running: true
                    NumberAnimation {
                        to: 0.4
                        duration: pinItem.isActive ? 400 : 1000 + pinItem.index * 180
                        easing.type: Easing.InOutSine
                    }
                    NumberAnimation {
                        to: 1.0
                        duration: pinItem.isActive ? 400 : 1000 + pinItem.index * 180
                        easing.type: Easing.InOutSine
                    }
                }

                // Outer glow ring — expands and turns white when active
                Rectangle {
                    anchors.centerIn: parent
                    width:   pinItem.isActive ? 24 : 14
                    height:  pinItem.isActive ? 24 : 14
                    radius:  width / 2
                    color:   pinItem.isActive ? "#00b4ff" : "#ff8800"
                    opacity: pinItem.pulseOpacity * (pinItem.isActive ? 0.45 : 0.30)
                    Behavior on width  { NumberAnimation { duration: 500; easing.type: Easing.OutQuad } }
                    Behavior on height { NumberAnimation { duration: 500; easing.type: Easing.OutQuad } }
                    Behavior on color  { ColorAnimation  { duration: 400 } }
                }

                // Pin dot — turns cyan when active
                Rectangle {
                    anchors.centerIn: parent
                    width:  pinItem.isActive ? 9 : 6
                    height: pinItem.isActive ? 9 : 6
                    radius: width / 2
                    color:  pinItem.isActive ? "#00b4ff" : "#ff8800"
                    opacity: pinItem.pulseOpacity
                    Behavior on width  { NumberAnimation { duration: 500; easing.type: Easing.OutQuad } }
                    Behavior on height { NumberAnimation { duration: 500; easing.type: Easing.OutQuad } }
                    Behavior on color  { ColorAnimation  { duration: 400 } }
                }
            }
        }
    }

    // ── Edge fades — outside the zoom container so they don't scale ───────────

    Rectangle {
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: parent.height * 0.10
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#ff070c18" }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: parent.height * 0.10
        gradient: Gradient {
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 1.0; color: "#ff070c18" }
        }
    }
}
