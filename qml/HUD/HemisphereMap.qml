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
    property real scrollSpeed: 0.2

    /**
     * @brief Server-driven map focus from a MapFocus WebSocket message.
     * When non-null, the globe pans and zooms to this location; auto-scroll pauses.
     * Set to null (MapClear) to zoom out and resume auto-scroll.
     * Format: { lat: number, lon: number, zoom: string }
     * Zoom string values: "continent" (1.2×), "country" (1.8×), "region" (2.8×), "city" (4.0×).
     */
    property var serverFocus: null

    // ── Day / night state ─────────────────────────────────────────────────────

    /**
     * @brief True when the user's local clock is between 06:00 and 20:00.
     * Drives the crossfade between the NASA Blue Marble (daytime) and
     * Black Marble (nighttime) map layers. Re-evaluated every 60 s.
     */
    property bool isDaytime: false

    readonly property string daySource:   "qrc:/resources/maps/world_map_day.jpg"
    readonly property string nightSource: "qrc:/resources/maps/world_map_night.jpg"

    /**
     * @brief Recompute isDaytime from the current local clock.
     */
    function updateDayNight() {
        var h = new Date().getHours()
        isDaytime = (h >= 6 && h < 20)
    }

    Component.onCompleted: updateDayNight()

    Timer {
        interval: 60000   // re-check every minute
        repeat:   true
        running:  true
        onTriggered: root.updateDayNight()
    }

    /**
     * @brief Maps a zoom level string from the server to a map scale factor.
     * @param zoomLevel "continent", "country", "region", or "city"
     * @return the corresponding scale multiplier
     */
    function zoomLevelToScale(zoomLevel) {
        if (zoomLevel === "continent") return 1.2
        if (zoomLevel === "country")   return 1.8
        if (zoomLevel === "region")    return 2.8
        return 4.0  // city (default)
    }

    // ── Internal state ────────────────────────────────────────────────────────

    // Standard equirectangular projection: left edge = -180°, right edge = +180°.
    // basePinX = (lon + 180) / 360 * width → lonShift must be 180.
    readonly property real lonShift: 180.0

    property real mapOffset:        0      // in (0, 2*width); initialised after layout
    property real zoomScale:        1.0
    property real mapVerticalOffset: 0.0  // pixels: shifts container to centre pin latitude
    property real targetFocusZoom:  1.6   // target zoom for next focusSequence run

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

            // Compute vertical offset to centre the pin's latitude.
            // With mapContainer scaled 1.6× around Item.Center:
            //   screen_y = mapVerticalOffset + height/2 + (pinY - height/2) * scale
            // Solving for screen_y = height/2:
            //   mapVerticalOffset = (height/2 - pinY) * targetScale
            var pinY = (1.0 - (pin.lat + 90) / 180) * root.height
            root.targetFocusZoom = 1.6
            seqVertPan.to = (root.height / 2 - pinY) * root.targetFocusZoom

            focusSequence.start()
        } else {
            // Clear: zoom out and resume auto-scroll.
            zoomOutAnim.start()
        }
    }

    // ── Server-driven focus (MapFocus / MapClear) ─────────────────────────────

    onServerFocusChanged: {
        focusSequence.stop()
        zoomOutAnim.stop()

        if (serverFocus !== null) {
            var sf       = serverFocus
            var shifted  = sf.lon + lonShift
            var normLon  = ((shifted % 360) + 360) % 360
            var basePinX = (normLon / 360) * root.width
            var target   = root.width / 2 + basePinX

            // Shortest-path wrap
            var diff = target - root.mapOffset
            if (Math.abs(diff - root.width) < Math.abs(diff)) diff -= root.width
            if (Math.abs(diff + root.width) < Math.abs(diff)) diff += root.width

            seqPan.to = root.mapOffset + diff

            var scale = zoomLevelToScale(sf.zoom)
            root.targetFocusZoom = scale
            var pinY = (1.0 - (sf.lat + 90) / 180) * root.height
            seqVertPan.to = (root.height / 2 - pinY) * scale

            focusSequence.start()
        } else {
            zoomOutAnim.start()
        }
    }

    // ── Animations ────────────────────────────────────────────────────────────

    /**
     * @brief Three-phase focus sequence: zoom out → (pan + delayed zoom-in).
     *
     * Phase 1 pulls back to full globe and resets any previous vertical offset.
     * Phase 2 pans horizontally and vertically in parallel with a delayed zoom-in,
     * so arrival and zoom blend into a single fluid motion.
     */
    SequentialAnimation {
        id: focusSequence

        // Phase 1 — pull back to full globe, reset vertical offset
        ParallelAnimation {
            NumberAnimation {
                target: root; property: "zoomScale"
                to: 1.0; duration: 450; easing.type: Easing.InOutQuad
            }
            NumberAnimation {
                target: root; property: "mapVerticalOffset"
                to: 0.0; duration: 450; easing.type: Easing.InOutQuad
            }
        }

        // Phase 2 — pan (horizontal + vertical) and zoom-in overlap
        ParallelAnimation {

            // Horizontal pan to centre the pin
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

            // Vertical pan to centre the pin's latitude (target set in onActivePinIndexChanged)
            NumberAnimation {
                id: seqVertPan
                target: root; property: "mapVerticalOffset"
                duration: 1300; easing.type: Easing.InOutCubic
            }

            // Zoom-in begins halfway through the pan (650 ms delay)
            SequentialAnimation {
                PauseAnimation { duration: 650 }
                NumberAnimation {
                    target: root; property: "zoomScale"
                    to: root.targetFocusZoom; duration: 850; easing.type: Easing.OutCubic
                }
            }
        }
    }

    /** @brief Zoom out, reset vertical offset, and resume globe rotation. */
    ParallelAnimation {
        id: zoomOutAnim
        NumberAnimation {
            target: root; property: "zoomScale"
            to: 1.0; duration: 600; easing.type: Easing.InOutQuad
        }
        NumberAnimation {
            target: root; property: "mapVerticalOffset"
            to: 0.0; duration: 600; easing.type: Easing.InOutQuad
        }
    }

    // ── Auto-scroll engine ────────────────────────────────────────────────────

    Timer {
        interval: 16
        repeat:   true
        running:  root.activePinIndex < 0 && root.serverFocus === null
                  && root.width > 0 && !focusSequence.running

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
        y:      root.mapVerticalOffset
        scale:  root.zoomScale
        transformOrigin: Item.Center   // expand toward all edges equally

        // ── Night map copies (NASA Black Marble) ──────────────────────────────

        // Left neighbour — night
        Image {
            x: -root.mapOffset
            width: root.width; height: root.height
            source: root.nightSource
            sourceSize: Qt.size(8192, 4096)
            asynchronous: true
            fillMode: Image.Stretch; smooth: true; antialiasing: true
            opacity: root.isDaytime ? 0.0 : 1.0
            Behavior on opacity { NumberAnimation { duration: 2000; easing.type: Easing.InOutQuad } }
        }

        // Centre copy — night
        Image {
            x: root.width - root.mapOffset
            width: root.width; height: root.height
            source: root.nightSource
            sourceSize: Qt.size(8192, 4096)
            asynchronous: true
            fillMode: Image.Stretch; smooth: true; antialiasing: true
            opacity: root.isDaytime ? 0.0 : 1.0
            Behavior on opacity { NumberAnimation { duration: 2000; easing.type: Easing.InOutQuad } }
        }

        // Right neighbour — night
        Image {
            x: 2 * root.width - root.mapOffset
            width: root.width; height: root.height
            source: root.nightSource
            sourceSize: Qt.size(8192, 4096)
            asynchronous: true
            fillMode: Image.Stretch; smooth: true; antialiasing: true
            opacity: root.isDaytime ? 0.0 : 1.0
            Behavior on opacity { NumberAnimation { duration: 2000; easing.type: Easing.InOutQuad } }
        }

        // ── Day map copies (NASA Blue Marble) ─────────────────────────────────

        // Left neighbour — day
        Image {
            x: -root.mapOffset
            width: root.width; height: root.height
            source: root.daySource
            sourceSize: Qt.size(8192, 4096)
            asynchronous: true
            fillMode: Image.Stretch; smooth: true; antialiasing: true
            opacity: root.isDaytime ? 1.0 : 0.0
            Behavior on opacity { NumberAnimation { duration: 2000; easing.type: Easing.InOutQuad } }
        }

        // Centre copy — day
        Image {
            x: root.width - root.mapOffset
            width: root.width; height: root.height
            source: root.daySource
            sourceSize: Qt.size(8192, 4096)
            asynchronous: true
            fillMode: Image.Stretch; smooth: true; antialiasing: true
            opacity: root.isDaytime ? 1.0 : 0.0
            Behavior on opacity { NumberAnimation { duration: 2000; easing.type: Easing.InOutQuad } }
        }

        // Right neighbour — day
        Image {
            x: 2 * root.width - root.mapOffset
            width: root.width; height: root.height
            source: root.daySource
            sourceSize: Qt.size(8192, 4096)
            asynchronous: true
            fillMode: Image.Stretch; smooth: true; antialiasing: true
            opacity: root.isDaytime ? 1.0 : 0.0
            Behavior on opacity { NumberAnimation { duration: 2000; easing.type: Easing.InOutQuad } }
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
