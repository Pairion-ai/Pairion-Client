/**
 * @file AdsbRadarScene.qml
 * @brief ADS-B radar scene — live aircraft overlay on an embedded FAA VFR sectional chart.
 *
 * Displays aircraft positions received via SceneDataPush (modelId="adsb") on top of
 * a pre-processed FAA VFR Sectional chart image embedded as a Qt resource. The image
 * (resources/adsb/vfr_sectional.png) covers exactly the ADS-B bounding box:
 *   lamin=33.681  lomin=-96.715  lamax=33.947  lomax=-96.449
 *
 * Coordinate mapping is a simple linear projection — no Mercator math required because
 * the image already spans the exact same bounding box as the aircraft data:
 *   screenX = (lon - lomin) / (lomax - lomin) * width
 *   screenY = (lamax - lat) / (lamax - lamin) * height
 *
 * Aircraft rendering:
 *   - Airborne GA aircraft: arrow silhouette (cyan) rotated to heading.
 *   - Airborne airline aircraft: arrow silhouette (amber) rotated to heading.
 *     Airline detection: callsign matches 2–3 uppercase letters followed by a digit.
 *   - On-ground aircraft: 6 px filled dot in the aircraft colour.
 *
 * Callout boxes (airborne aircraft only):
 *   - Line 1: identifier — registration if available, else ICAO24.
 *   - Line 2: aircraft type / speed knots / altitude ft.
 *   - Line 3 (airlines with route only): origin→destination.
 *   A thin connector line runs from the icon centre to the callout panel.
 *
 * Range rings are drawn at 2 nm, 4 nm, and 8 nm from the home location at low
 * opacity (0.25) so the sectional chart remains the dominant visual layer.
 *
 * To regenerate the sectional PNG after an FAA chart cycle (every 56 days), run:
 *   python3 scripts/process_vfr_sectional.py
 */
import QtQuick
import Pairion
import PairionScene 1.0

Item {
    id: root

    // ── SceneBase contract ────────────────────────────────────────

    /** Server-pushed model data keyed by modelId. */
    property var    sceneData:   ({})
    /** Parameters from the activating SceneChange command. */
    property var    sceneParams: ({})
    /** Current agent state string from SceneManager. */
    property string hudState:    "idle"

    /** @brief Request scene switch (forwarded to SceneManager when needed). */
    signal requestScene(string sceneId, var params)
    /** @brief Request TTS narration. */
    signal requestSpeak(string text)

    // ── Bounding box constants (must match application.yml pairion.data.adsb) ──

    /** @brief Southern boundary of the ADS-B coverage area (degrees). */
    readonly property real lamin:  33.681
    /** @brief Western boundary of the ADS-B coverage area (degrees). */
    readonly property real lomin: -96.715
    /** @brief Northern boundary of the ADS-B coverage area (degrees). */
    readonly property real lamax:  33.947
    /** @brief Eastern boundary of the ADS-B coverage area (degrees). */
    readonly property real lomax: -96.449

    /** @brief Home location latitude — centre of range rings. */
    readonly property real homeLat: 33.814427
    /** @brief Home location longitude — centre of range rings. */
    readonly property real homeLon: -96.582106

    // ── Aircraft data ─────────────────────────────────────────────

    /**
     * @brief Live aircraft array from SceneDataPush (modelId="adsb").
     * Each element is an object with fields: icao24, callsign, lat, lon,
     * altitudeFt, speedKnots, trackDeg, onGround, registration,
     * aircraftType, origin, destination.
     */
    readonly property var aircraftList: {
        var raw = sceneData["adsb"]
        if (Array.isArray(raw)) return raw
        return []
    }

    // ── Coordinate helpers ────────────────────────────────────────

    /**
     * @brief Maps a longitude to a screen X pixel using linear interpolation
     * across the bounding box.
     * @param lon Longitude in decimal degrees.
     * @return Screen X in pixels.
     */
    function lonToScreenX(lon) {
        return (lon - lomin) / (lomax - lomin) * width
    }

    /**
     * @brief Maps a latitude to a screen Y pixel using linear interpolation
     * across the bounding box. Latitude is inverted: higher lat → smaller Y.
     * @param lat Latitude in decimal degrees.
     * @return Screen Y in pixels.
     */
    function latToScreenY(lat) {
        return (lamax - lat) / (lamax - lamin) * height
    }

    /**
     * @brief Returns true when the callsign matches the ICAO airline pattern
     * (2–3 uppercase letters followed by a digit).
     * @param callsign The aircraft callsign string.
     * @return True for airline flights, false for GA or unknown.
     */
    function isAirline(callsign) {
        if (!callsign) return false
        return /^[A-Z]{2,3}\d/.test(callsign)
    }

    /**
     * @brief Returns the display identifier for an aircraft.
     * Prefers registration over ICAO24 hex address.
     * @param ac Aircraft object.
     * @return Display identifier string.
     */
    function aircraftId(ac) {
        return (ac.registration && ac.registration !== "") ? ac.registration : ac.icao24
    }

    /**
     * @brief Returns the type/speed/alt summary line for an aircraft.
     * @param ac Aircraft object.
     * @return Formatted string, e.g. "B738  480kt  35000ft".
     */
    function aircraftSummary(ac) {
        var type  = (ac.aircraftType && ac.aircraftType !== "") ? ac.aircraftType : "----"
        var speed = ac.speedKnots ? Math.round(ac.speedKnots) + "kt" : "---"
        var alt   = ac.altitudeFt ? Math.round(ac.altitudeFt / 100) * 100 + "ft" : "---"
        return type + "  " + speed + "  " + alt
    }

    /**
     * @brief Returns the route string for an aircraft if both endpoints are known.
     * @param ac Aircraft object.
     * @return "KDFW→KLAX" style string, or empty string if unavailable.
     */
    function aircraftRoute(ac) {
        if (ac.origin && ac.destination && ac.origin !== "" && ac.destination !== "")
            return ac.origin + "→" + ac.destination
        return ""
    }

    // ── Layers ────────────────────────────────────────────────────

    // Dark base — shown during image load and behind transparent PNG edges.
    Rectangle {
        anchors.fill: parent
        color: PairionStyle.darkBg
    }

    // FAA VFR Sectional chart — pre-processed, dark-blue tinted, covers the bounding box exactly.
    Image {
        id: chartImage
        anchors.fill: parent
        source:   "qrc:/resources/adsb/vfr_sectional.png"
        fillMode: Image.Stretch
        smooth:   true
        // Fade in once loaded to avoid a jarring black flash.
        opacity: status === Image.Ready ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: 400 } }
    }

    // Range rings — drawn at 2 nm, 4 nm, 8 nm from home at low opacity.
    Canvas {
        id: rings
        anchors.fill: parent
        opacity: 0.25

        /**
         * @brief Draws concentric range rings from the home location.
         *
         * Pixel radius is derived from the latitude scale:
         *   pxPerNm = height / (lamax - lamin) / 60
         * This gives slightly elliptical circles because longitude pixels differ,
         * which is acceptable at this scale for range reference purposes.
         */
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            // Pixels per nautical mile in the Y direction.
            var pxPerNm = height / (root.lamax - root.lamin) / 60.0

            var cx = root.lonToScreenX(root.homeLon)
            var cy = root.latToScreenY(root.homeLat)

            ctx.strokeStyle = "#00b4ff"
            ctx.lineWidth   = 1
            ctx.setLineDash([4, 6])
            ctx.font        = "9px 'Courier New'"
            ctx.fillStyle   = "#00b4ff"
            ctx.textAlign   = "left"

            var ringSets = [
                { nm: 2, label: "2nm"  },
                { nm: 4, label: "4nm"  },
                { nm: 8, label: "8nm"  }
            ]

            for (var i = 0; i < ringSets.length; i++) {
                var r = ringSets[i].nm * pxPerNm
                ctx.beginPath()
                ctx.arc(cx, cy, r, 0, 2 * Math.PI)
                ctx.stroke()
                ctx.fillText(ringSets[i].label, cx + 4, cy - r + 12)
            }

            // Centre crosshair at home location.
            ctx.strokeStyle = "#00b4ff"
            ctx.lineWidth   = 1
            ctx.setLineDash([2, 4])
            ctx.beginPath()
            ctx.moveTo(cx - 10, cy); ctx.lineTo(cx + 10, cy)
            ctx.moveTo(cx, cy - 10); ctx.lineTo(cx, cy + 10)
            ctx.stroke()
            ctx.setLineDash([])
        }

        Component.onCompleted: requestPaint()
        onWidthChanged:  requestPaint()
        onHeightChanged: requestPaint()
    }

    // Aircraft icon layer — single Canvas redrawn on each data update.
    Canvas {
        id: iconCanvas
        anchors.fill: parent

        property var aircraft: root.aircraftList

        /**
         * @brief Redraws all aircraft icons whenever the aircraft list changes.
         *
         * On-ground aircraft are drawn as 6 px filled dots.
         * Airborne aircraft are drawn as arrow silhouettes rotated to heading.
         * Colour: PairionStyle.gaColor (cyan) for GA, PairionStyle.airlineColor (amber) for airline.
         */
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            var list = aircraft
            if (!list || !list.length) return

            for (var i = 0; i < list.length; i++) {
                var ac  = list[i]
                var sx  = root.lonToScreenX(ac.lon)
                var sy  = root.latToScreenY(ac.lat)

                // Skip aircraft outside the visible viewport with a small margin.
                if (sx < -30 || sx > width + 30 || sy < -30 || sy > height + 30)
                    continue

                var airline = root.isAirline(ac.callsign)
                var color   = airline ? PairionStyle.airlineColor : PairionStyle.gaColor

                ctx.fillStyle   = color
                ctx.strokeStyle = color

                if (ac.onGround) {
                    // On-ground: small filled dot.
                    ctx.beginPath()
                    ctx.arc(sx, sy, 3, 0, 2 * Math.PI)
                    ctx.fill()
                } else {
                    // Airborne: arrow silhouette rotated to heading (trackDeg, 0 = north).
                    var heading = (ac.trackDeg || 0) * Math.PI / 180.0

                    ctx.save()
                    ctx.translate(sx, sy)
                    ctx.rotate(heading)

                    // Arrow: tip at (0, -10), wings at (±6, +8), tail notch at (0, +4).
                    ctx.beginPath()
                    ctx.moveTo(0,  -10)
                    ctx.lineTo(6,    8)
                    ctx.lineTo(0,    4)
                    ctx.lineTo(-6,   8)
                    ctx.closePath()
                    ctx.fill()

                    ctx.restore()
                }
            }
        }

        onAircraftChanged: requestPaint()
        onWidthChanged:    requestPaint()
        onHeightChanged:   requestPaint()
    }

    // Callout boxes — one Item per airborne aircraft.
    Repeater {
        model: root.aircraftList.filter(function(ac) { return !ac.onGround })

        /**
         * @brief Callout panel for one airborne aircraft.
         *
         * Positioned 16 px right and above the icon centre. A thin connector
         * line runs from the icon centre to the panel's bottom-left corner.
         */
        delegate: Item {
            id: calloutRoot

            required property var modelData
            readonly property var  ac:       modelData
            readonly property real iconX:    root.lonToScreenX(ac.lon)
            readonly property real iconY:    root.latToScreenY(ac.lat)
            readonly property real boxX:     iconX + 16
            readonly property real boxY:     iconY - 26 - calloutBox.height
            readonly property bool airline:  root.isAirline(ac.callsign)
            readonly property color acColor: airline ? PairionStyle.airlineColor : PairionStyle.gaColor
            readonly property string route:  root.aircraftRoute(ac)

            // Connector line from icon centre to callout box bottom-left corner.
            Canvas {
                id: connector
                anchors.fill: parent

                /**
                 * @brief Draws a 1 px connector from the icon centre to the callout box.
                 */
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.strokeStyle  = calloutRoot.acColor
                    ctx.globalAlpha  = 0.6
                    ctx.lineWidth    = 1
                    ctx.beginPath()
                    ctx.moveTo(calloutRoot.iconX, calloutRoot.iconY)
                    ctx.lineTo(calloutRoot.boxX, calloutRoot.boxY + calloutBox.height)
                    ctx.stroke()
                }

                Component.onCompleted: requestPaint()
            }

            // Callout box panel.
            Rectangle {
                id: calloutBox
                x: calloutRoot.boxX
                y: calloutRoot.boxY
                width:  contentCol.implicitWidth  + 8
                height: contentCol.implicitHeight + 6
                color:        Qt.rgba(0.05, 0.09, 0.14, 0.85)
                border.color: calloutRoot.acColor
                border.width: 1
                radius:       2
                visible:      calloutRoot.iconX > -50 && calloutRoot.iconX < root.width  + 50
                           && calloutRoot.iconY > -50 && calloutRoot.iconY < root.height + 50

                Column {
                    id: contentCol
                    x: 4; y: 3
                    spacing: 1

                    // Line 1: identifier (registration or ICAO24).
                    Text {
                        text:  root.aircraftId(calloutRoot.ac)
                        color: calloutRoot.acColor
                        font.family:    PairionStyle.fontFamily
                        font.pixelSize: PairionStyle.fontSizeSm
                        font.bold:      true
                    }

                    // Line 2: type / speed / altitude.
                    Text {
                        text:  root.aircraftSummary(calloutRoot.ac)
                        color: PairionStyle.dim
                        font.family:    PairionStyle.fontFamily
                        font.pixelSize: PairionStyle.fontSizeSm
                    }

                    // Line 3: route — shown for airline flights with route data only.
                    Text {
                        visible: calloutRoot.airline && calloutRoot.route !== ""
                        text:    calloutRoot.route
                        color:   PairionStyle.dim
                        font.family:    PairionStyle.fontFamily
                        font.pixelSize: PairionStyle.fontSizeXs
                    }
                }
            }
        }
    }

    // No-data overlay — shown when aircraft list is empty.
    Text {
        anchors.centerIn: parent
        visible: root.aircraftList.length === 0
        text:    "No ADS-B data"
        color:   "#305070"
        font.family:    PairionStyle.fontFamily
        font.pixelSize: PairionStyle.fontSizeLg
    }
}
