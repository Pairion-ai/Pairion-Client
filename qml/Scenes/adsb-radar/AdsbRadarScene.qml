/**
 * @file AdsbRadarScene.qml
 * @brief ADS-B radar scene — live aircraft overlay on a VFR sectional chart background.
 *
 * Displays aircraft positions received via SceneDataPush (modelId="adsb") on top of
 * FAA VFR Sectional chart tiles fetched from the ArcGIS Online tile server. If tiles
 * fail to load, concentric range rings serve as the background reference.
 *
 * Aircraft rendering:
 *   - Airborne GA aircraft: 20×20 cyan arrow silhouette rotated to heading.
 *   - Airborne airline aircraft: 20×20 amber arrow silhouette rotated to heading.
 *     Airline detection uses the callsign pattern: 2–3 uppercase letters followed by digits.
 *   - On-ground aircraft: 6×6 filled dot in the aircraft colour.
 *
 * Callout boxes (airborne aircraft only):
 *   - Line 1: identifier — registration if available, else ICAO24.
 *   - Line 2: aircraft type / speed knots / altitude ft.
 *   - Line 3 (airlines with route only): origin→destination.
 *   A connector line runs from the icon centre to the callout box.
 *
 * Map centre and zoom are driven by sceneParams:
 *   - centerLat  (default 39.5 — continental US centre)
 *   - centerLon  (default -98.35)
 *   - zoom       (default 8 — roughly 50 nm = 152 px)
 *
 * The tile layer uses a 7×7 grid of 256 px tiles centered on the computed centre tile,
 * giving full coverage for typical screen sizes at zoom 8. Tiles are loaded lazily;
 * a loading-failure is invisible because the range rings and dark background show through.
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

    // ── Map parameters ────────────────────────────────────────────

    /** @brief Map centre latitude in decimal degrees. */
    readonly property real centerLat: sceneParams["centerLat"] !== undefined
                                      ? sceneParams["centerLat"] : 39.5

    /** @brief Map centre longitude in decimal degrees. */
    readonly property real centerLon: sceneParams["centerLon"] !== undefined
                                      ? sceneParams["centerLon"] : -98.35

    /** @brief Tile zoom level (1–12). Default 8 gives ~50 nm ≈ 152 px per ring. */
    readonly property int  mapZoom:   sceneParams["zoom"] !== undefined
                                      ? parseInt(sceneParams["zoom"]) : 8

    // ── Derived map geometry ──────────────────────────────────────

    /** @brief Web Mercator world-pixel width at the current zoom level. */
    readonly property real worldSize: 256.0 * Math.pow(2.0, mapZoom)

    /** @brief World-pixel X coordinate of the map centre. */
    readonly property real centerPxX: (centerLon + 180.0) / 360.0 * worldSize

    /** @brief World-pixel Y coordinate of the map centre (Mercator). */
    readonly property real centerPxY: {
        var sinLat = Math.sin(centerLat * Math.PI / 180.0)
        return (0.5 - Math.log((1.0 + sinLat) / (1.0 - sinLat)) / (4.0 * Math.PI)) * worldSize
    }

    /** @brief Column index of the tile that contains the map centre. */
    readonly property int centerTileX: Math.floor(centerPxX / 256)

    /** @brief Row index of the tile that contains the map centre. */
    readonly property int centerTileY: Math.floor(centerPxY / 256)

    /**
     * @brief Screen X of the top-left corner of the centre tile.
     * Placed so that the map centre pixel aligns with the screen centre.
     */
    readonly property real tileOriginX: width  / 2.0 - (centerPxX - centerTileX * 256)

    /**
     * @brief Screen Y of the top-left corner of the centre tile.
     * Placed so that the map centre pixel aligns with the screen centre.
     */
    readonly property real tileOriginY: height / 2.0 - (centerPxY - centerTileY * 256)

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

    // ── Helpers ───────────────────────────────────────────────────

    /**
     * @brief Returns the screen X position for a given longitude.
     * @param lon Longitude in decimal degrees.
     * @return Screen X in pixels.
     */
    function lonToScreenX(lon) {
        var pxX = (lon + 180.0) / 360.0 * worldSize
        return width / 2.0 + (pxX - centerPxX)
    }

    /**
     * @brief Returns the screen Y position for a given latitude.
     * @param lat Latitude in decimal degrees.
     * @return Screen Y in pixels.
     */
    function latToScreenY(lat) {
        var sinLat = Math.sin(lat * Math.PI / 180.0)
        var pxY = (0.5 - Math.log((1.0 + sinLat) / (1.0 - sinLat)) / (4.0 * Math.PI)) * worldSize
        return height / 2.0 + (pxY - centerPxY)
    }

    /**
     * @brief Returns true when the callsign matches the ICAO airline pattern (2–3 letters + digits).
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
     * @brief Returns the route string for an aircraft if origin and destination are known.
     * @param ac Aircraft object.
     * @return "KDFW→KLAX" style string, or empty string if not available.
     */
    function aircraftRoute(ac) {
        if (ac.origin && ac.destination && ac.origin !== "" && ac.destination !== "")
            return ac.origin + "→" + ac.destination
        return ""
    }

    // ── Tile model (7×7 grid, radius 3 in each direction) ────────

    /**
     * @brief Generates the 49-element tile grid model from the current centre tile.
     * Each element carries the tile column, row, and screen offset from the tile origin.
     * @return Array of {col, row, dx, dy} objects.
     */
    function buildTileModel() {
        var tiles = []
        for (var dy = -3; dy <= 3; dy++) {
            for (var dx = -3; dx <= 3; dx++) {
                tiles.push({ col: centerTileX + dx,
                              row: centerTileY + dy,
                              dx:  dx * 256,
                              dy:  dy * 256 })
            }
        }
        return tiles
    }

    property var tileModel: buildTileModel()

    // Rebuild tile grid when map parameters change.
    onCenterLatChanged: tileModel = buildTileModel()
    onCenterLonChanged: tileModel = buildTileModel()
    onMapZoomChanged:   tileModel = buildTileModel()

    // ── Layers ────────────────────────────────────────────────────

    // Dark base — always visible, shows through tile-load failures.
    Rectangle {
        anchors.fill: parent
        color: PairionStyle.darkBg
    }

    // FAA VFR Sectional chart tile layer.
    Item {
        id: tileLayer
        anchors.fill: parent
        clip: true

        Repeater {
            model: root.tileModel

            /**
             * @brief Single map tile fetched from the FAA VFR sectional tile server.
             * URL format: ArcGIS Online tile service — {z}/{y}/{x} ordering.
             */
            Image {
                x: root.tileOriginX + modelData.dx
                y: root.tileOriginY + modelData.dy
                width: 256
                height: 256
                source: "https://tiles.arcgisonline.com/arcgis/rest/services/Specialty/FAA_Sectional_VFR/MapServer/tile/"
                        + root.mapZoom + "/" + modelData.row + "/" + modelData.col
                asynchronous: true
                cache: true
                // Tiles fade in to avoid jarring pop-in.
                opacity: status === Image.Ready ? 1.0 : 0.0
                Behavior on opacity { NumberAnimation { duration: 300 } }
            }
        }

        // Semi-transparent dark tint over the tiles for contrast with aircraft symbols.
        Rectangle {
            anchors.fill: parent
            color: "#070c18"
            opacity: 0.55
        }
    }

    // Range rings — always visible as background reference.
    Canvas {
        id: rings
        anchors.fill: parent

        /**
         * @brief Draws concentric range rings at 50 nm, 100 nm, and 200 nm.
         * Ring radii are computed in world pixels and converted to screen pixels.
         * 1 nm = 1/60 of a degree of latitude; Mercator scale is approximately
         * constant near the map centre for the precision needed here.
         */
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            // Pixels per nautical mile at map centre (approx. — latitude-corrected).
            var pxPerDeg = root.worldSize / 360.0
            var pxPerNm  = pxPerDeg / 60.0

            var cx = width  / 2.0
            var cy = height / 2.0

            ctx.strokeStyle = "#204060"
            ctx.lineWidth   = 1
            ctx.setLineDash([4, 6])
            ctx.font        = "9px 'Courier New'"
            ctx.fillStyle   = "#3a6080"
            ctx.textAlign   = "left"

            var rings = [
                { nm: 50,  label: "50nm"  },
                { nm: 100, label: "100nm" },
                { nm: 200, label: "200nm" }
            ]

            for (var i = 0; i < rings.length; i++) {
                var r = rings[i].nm * pxPerNm
                ctx.beginPath()
                ctx.arc(cx, cy, r, 0, 2 * Math.PI)
                ctx.stroke()

                // Label at the 12 o'clock position of each ring.
                ctx.fillText(rings[i].label, cx + 4, cy - r + 12)
            }

            // Centre crosshair.
            ctx.strokeStyle = "#305070"
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

    // Aircraft icon layer — one Canvas drawn over all aircraft icons together.
    Canvas {
        id: iconCanvas
        anchors.fill: parent

        property var aircraft: root.aircraftList

        /**
         * @brief Redraws all aircraft icons whenever the aircraft list changes.
         *
         * On-ground aircraft are drawn as 6×6 filled dots.
         * Airborne aircraft are drawn as 20×20 arrow silhouettes rotated to heading.
         * Colour: PairionStyle.gaColor for GA, PairionStyle.airlineColor for airline.
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

                    // Arrow: tip at (0, -10), base wings at (±6, +8), tail notch at (0, +4).
                    ctx.beginPath()
                    ctx.moveTo(0, -10)       // tip
                    ctx.lineTo(6,   8)        // right wing
                    ctx.lineTo(0,   4)        // tail notch
                    ctx.lineTo(-6,  8)        // left wing
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

    // Callout boxes — one Item per airborne aircraft with callsign data.
    Repeater {
        model: root.aircraftList.filter(function(ac) { return !ac.onGround })

        /**
         * @brief Callout panel for one airborne aircraft.
         *
         * Positioned 16 px right and 26 px above the icon centre. A thin connector
         * line runs from the icon centre to the panel corner.
         */
        delegate: Item {
            id: calloutRoot

            required property var modelData
            readonly property var  ac:      modelData
            readonly property real iconX:   root.lonToScreenX(ac.lon)
            readonly property real iconY:   root.latToScreenY(ac.lat)
            readonly property real boxX:    iconX + 16
            readonly property real boxY:    iconY - 26 - calloutBox.height
            readonly property bool airline: root.isAirline(ac.callsign)
            readonly property color acColor: airline ? PairionStyle.airlineColor : PairionStyle.gaColor
            readonly property string route: root.aircraftRoute(ac)

            // Connector line from icon centre to callout box bottom-left corner.
            Canvas {
                id: connector
                anchors.fill: parent

                /**
                 * @brief Draws a thin 1 px connector from the icon centre to the box corner.
                 */
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.strokeStyle = calloutRoot.acColor
                    ctx.globalAlpha = 0.6
                    ctx.lineWidth   = 1
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
                color:           Qt.rgba(0.05, 0.09, 0.14, 0.85)
                border.color:    calloutRoot.acColor
                border.width:    1
                radius:          2
                visible:         calloutRoot.iconX > -50 && calloutRoot.iconX < root.width  + 50
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
