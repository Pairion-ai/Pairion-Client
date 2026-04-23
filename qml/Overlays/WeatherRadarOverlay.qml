/**
 * @file WeatherRadarOverlay.qml
 * @brief Precipitation radar overlay using RainViewer tile data.
 *
 * Fetches the latest radar frames from the RainViewer public API and renders
 * them as semi-transparent precipitation tiles on top of the active background.
 * Works with any background that implements latLonToScreen(lat, lon) — tiles are
 * positioned using Approach A: each tile's NW and SE corners are converted from
 * geographic coordinates to screen pixels via background.latLonToScreen().
 *
 * Animation: cycles two frames — older frame displayed 500 ms, then a 200 ms
 * crossfade to the latest frame held for 1500 ms, then crossfade back.
 *
 * Legend: precipitation colour scale in the bottom-right corner.
 *
 * API: RainViewer public weather maps — no API key required.
 *   https://api.rainviewer.com/public/weather-maps.json
 *
 * Tile URL pattern:
 *   {host}{path}/512/{z}/{x}/{y}/4/1_1.png
 *   colorScheme 4  = meteorological rainbow
 *   options     1_1 = smooth data, snow highlighted
 *
 * Parameters read from overlayParams:
 * - zoom: radar tile zoom level 4–8 (default: 6 — continental/regional scale)
 */
import QtQuick
import PairionScene 1.0

Item {
    id: root

    // ── OverlayBase contract ──────────────────────────────────────

    /**
     * @brief Reference to the active background instance.
     * Calls background.latLonToScreen(lat, lon) for geographic tile positioning.
     */
    property var background: null

    /** @brief Parameters for this overlay from the most recent OverlayAdd command. */
    property var overlayParams: ({})

    /** @brief Model data keyed by modelId (not used by this overlay). */
    property var sceneData: ({})

    /** @brief Current agent state string from LayerManager. */
    property string hudState: "idle"

    /** @brief Request TTS narration. */
    signal requestSpeak(string text)

    // ── Radar data state ──────────────────────────────────────────

    /**
     * @brief RainViewer CDN host from the weather-maps API response.
     * Example: "https://tilecache.rainviewer.com"
     */
    property string radarHost: ""

    /**
     * @brief Up to two recent radar frames as [{time, path}].
     * frames[0] = older, frames[1] = latest (most recent).
     */
    property var frames: []

    /** @brief True once the API response has been parsed and tile lists built. */
    property bool dataLoaded: false

    /**
     * @brief Tile descriptor list for frames[0] (older frame).
     * Each element: { url: string, x: real, y: real, w: real, h: real }
     */
    property var tileListA: []

    /**
     * @brief Tile descriptor list for frames[1] (latest frame).
     * Each element: { url: string, x: real, y: real, w: real, h: real }
     */
    property var tileListB: []

    // ── Animation opacity properties ──────────────────────────────

    /** @brief Opacity of the older-frame tile layer, driven by frameAnim. */
    property real opacityA: 0.7

    /** @brief Opacity of the latest-frame tile layer, driven by frameAnim. */
    property real opacityB: 0.0

    // ── Lifecycle ─────────────────────────────────────────────────

    Component.onCompleted: _fetchRadarData()

    onBackgroundChanged:  { if (dataLoaded) _rebuildBothTileLists() }
    onWidthChanged:       { if (dataLoaded) _rebuildBothTileLists() }
    onHeightChanged:      { if (dataLoaded) _rebuildBothTileLists() }

    // ── Refresh timer (every 10 minutes) ─────────────────────────

    /**
     * @brief Polls the RainViewer API every 10 minutes for updated radar frames.
     */
    Timer {
        interval: 600000
        running:  true
        repeat:   true
        onTriggered: root._fetchRadarData()
    }

    // ── Frame-cycling animation ───────────────────────────────────

    /**
     * @brief Cycles between older and latest radar frames.
     *
     * Sequence (loops continuously when two frames are available):
     *   1. Show older frame at 0.7 opacity (500 ms)
     *   2. Crossfade to latest frame (200 ms)
     *   3. Hold latest frame at 0.7 opacity (1500 ms)
     *   4. Crossfade back to older frame (200 ms)
     */
    SequentialAnimation {
        id: frameAnim
        running: root.dataLoaded && root.frames.length >= 2
        loops:   Animation.Infinite

        // Show older frame.
        PropertyAction { target: root; property: "opacityA"; value: 0.7 }
        PropertyAction { target: root; property: "opacityB"; value: 0.0 }
        PauseAnimation { duration: 500 }

        // Crossfade to latest frame.
        ParallelAnimation {
            NumberAnimation { target: root; property: "opacityA"; to: 0.0; duration: 200 }
            NumberAnimation { target: root; property: "opacityB"; to: 0.7; duration: 200 }
        }

        // Hold latest frame.
        PauseAnimation { duration: 1500 }

        // Crossfade back to older frame.
        ParallelAnimation {
            NumberAnimation { target: root; property: "opacityB"; to: 0.0; duration: 200 }
            NumberAnimation { target: root; property: "opacityA"; to: 0.7; duration: 200 }
        }
    }

    // ── Private helpers ───────────────────────────────────────────

    /**
     * @brief Fetches the RainViewer public weather-maps manifest via XMLHttpRequest.
     *
     * On success, populates radarHost and frames (last two entries from radar.past),
     * sets dataLoaded = true, and calls _rebuildBothTileLists().
     */
    function _fetchRadarData() {
        var req = new XMLHttpRequest()
        req.onreadystatechange = function() {
            if (req.readyState !== 4) return
            if (req.status !== 200) {
                console.warn("WeatherRadarOverlay: API fetch failed, status", req.status)
                return
            }
            try {
                var data = JSON.parse(req.responseText)
                root.radarHost = data.host
                var past = data.radar.past
                if (!past || past.length === 0) {
                    console.warn("WeatherRadarOverlay: no radar frames in API response")
                    return
                }
                root.frames = past.length >= 2
                              ? [past[past.length - 2], past[past.length - 1]]
                              : [past[0]]
                root.dataLoaded = true
                root._rebuildBothTileLists()
            } catch (e) {
                console.warn("WeatherRadarOverlay: JSON parse error:", e)
            }
        }
        req.open("GET", "https://api.rainviewer.com/public/weather-maps.json")
        req.send()
    }

    /**
     * @brief Returns the geographic NW corner of tile (tx, ty) at zoom z.
     *
     * Uses standard Slippy Map / Web Mercator inverse projection.
     *
     * @param z  Tile zoom level.
     * @param tx Tile column index (may be negative or >= 2^z; caller wraps for the URL).
     * @param ty Tile row index (clamped to valid range by caller).
     * @return Plain JS object { lat: real, lon: real } in decimal degrees.
     */
    function _tileNW(z, tx, ty) {
        var n   = Math.pow(2, z)
        var lon = tx / n * 360 - 180
        var lat = Math.atan(Math.sinh(Math.PI * (1 - 2 * ty / n))) * 180 / Math.PI
        return { lat: lat, lon: lon }
    }

    /**
     * @brief Builds a tile descriptor list for one radar frame using Approach A positioning.
     *
     * For each tile in the viewport grid, the NW and SE corners are converted to screen
     * pixels via background.latLonToScreen(). This ensures precipitation blobs align
     * geographically regardless of which background is active.
     *
     * @param framePath Path component from the RainViewer API (e.g. "/v2/radar/1713024000").
     * @return Array of { url, x, y, w, h } objects ready for the Repeater delegates.
     */
    function _buildTileList(framePath) {
        if (!framePath || !root.background || root.radarHost === "") return []

        var vw = width  > 0 ? width  : 2560
        var vh = height > 0 ? height : 1440

        // Radar zoom: from overlayParams override, or default 6 (continental/regional).
        var z = overlayParams.zoom ? Math.floor(overlayParams.zoom) : 6
        z = Math.max(4, Math.min(z, 8))

        // Viewport centre from the active background's params, or fallback coordinates.
        var bgParams = root.background.backgroundParams || {}
        var cLat = bgParams.center_lat || 33.814427
        var cLon = bgParams.center_lon || -96.582106

        // Centre tile in fractional tile coordinates.
        var n      = Math.pow(2, z)
        var cLatR  = cLat * Math.PI / 180
        var ctxf   = (cLon + 180) / 360 * n
        var ctyf   = (1 - Math.log(Math.tan(cLatR) + 1 / Math.cos(cLatR)) / Math.PI) / 2 * n

        // Tile range: enough to fill viewport plus 2-tile margin on each side.
        var halfX = Math.ceil(vw / 256 / 2) + 2
        var halfY = Math.ceil(vh / 256 / 2) + 2

        var tiles = []
        for (var ty = Math.floor(ctyf) - halfY; ty <= Math.floor(ctyf) + halfY; ty++) {
            if (ty < 0 || ty >= n) continue
            for (var tx = Math.floor(ctxf) - halfX; tx <= Math.floor(ctxf) + halfX; tx++) {
                // NW corner (geographic) → screen pixel.
                var nw = _tileNW(z, tx,     ty)
                var se = _tileNW(z, tx + 1, ty + 1)
                var p1 = root.background.latLonToScreen(nw.lat, nw.lon)
                var p2 = root.background.latLonToScreen(se.lat, se.lon)
                var tw = p2.x - p1.x
                var th = p2.y - p1.y
                // Frustum cull: skip tiles entirely outside the viewport.
                if (p2.x < -tw || p1.x > vw + tw) continue
                if (p2.y < -th || p1.y > vh + th) continue
                // Wrap X for seamless antimeridian crossing.
                var wrappedTX = ((tx % n) + n) % n
                tiles.push({
                    url: root.radarHost + framePath + "/512/" + z + "/" + wrappedTX + "/" + ty + "/4/1_1.png",
                    x:   Math.round(p1.x),
                    y:   Math.round(p1.y),
                    w:   Math.round(tw),
                    h:   Math.round(th)
                })
            }
        }
        return tiles
    }

    /**
     * @brief Rebuilds tileListA (older frame) and tileListB (latest frame).
     * Called after the API response is received and whenever layout geometry changes.
     */
    function _rebuildBothTileLists() {
        if (root.frames.length === 0) return
        tileListA = _buildTileList(root.frames[0].path)
        tileListB = root.frames.length >= 2 ? _buildTileList(root.frames[1].path) : []
    }

    // ── Tile renderers ────────────────────────────────────────────

    /**
     * @brief Older radar frame tile layer.
     * Opacity is animated by frameAnim (0.7 while shown, 0 while faded out).
     */
    Repeater {
        model: root.tileListA

        delegate: Image {
            required property var modelData
            x:            modelData.x
            y:            modelData.y
            width:        modelData.w
            height:       modelData.h
            source:       modelData.url
            opacity:      root.opacityA
            cache:        true
            asynchronous: true
            smooth:       true
            fillMode:     Image.Stretch
        }
    }

    /**
     * @brief Latest radar frame tile layer.
     * Opacity is animated by frameAnim (0 while faded out, 0.7 while held).
     */
    Repeater {
        model: root.tileListB

        delegate: Image {
            required property var modelData
            x:            modelData.x
            y:            modelData.y
            width:        modelData.w
            height:       modelData.h
            source:       modelData.url
            opacity:      root.opacityB
            cache:        true
            asynchronous: true
            smooth:       true
            fillMode:     Image.Stretch
        }
    }

    // ── Precipitation legend ──────────────────────────────────────

    /**
     * @brief Precipitation intensity colour-scale legend.
     *
     * Displayed bottom-right. Shows a rainbow gradient bar (matching RainViewer
     * colour scheme 4) with Light / Moderate / Heavy labels and the timestamp of
     * the latest frame in UTC.
     */
    Rectangle {
        id: legend
        anchors.right:   parent.right
        anchors.bottom:  parent.bottom
        anchors.margins: 16
        width:   130
        height:  legendCol.implicitHeight + 12
        color:   Qt.rgba(0.05, 0.09, 0.14, 0.85)
        border.color: PairionStyle.borderColor
        border.width: PairionStyle.borderWidth
        radius:  PairionStyle.radiusSm
        visible: root.dataLoaded

        Column {
            id: legendCol
            anchors {
                left:   parent.left
                right:  parent.right
                top:    parent.top
                margins: 6
            }
            spacing: 4

            Text {
                text:           "Precipitation"
                color:          PairionStyle.dim
                font.family:    PairionStyle.fontFamily
                font.pixelSize: PairionStyle.fontSizeXs
                font.bold:      true
            }

            // Colour gradient bar — matches RainViewer colour scheme 4 (meteorological rainbow).
            Rectangle {
                width:  parent.width
                height: 10
                radius: 2
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0;  color: "#0000ff" }
                    GradientStop { position: 0.25; color: "#00ccff" }
                    GradientStop { position: 0.5;  color: "#00ff00" }
                    GradientStop { position: 0.75; color: "#ffff00" }
                    GradientStop { position: 1.0;  color: "#ff0000" }
                }
            }

            // Intensity labels below the bar.
            Row {
                width: parent.width
                Text {
                    text:           "Light"
                    color:          PairionStyle.dim
                    font.family:    PairionStyle.fontFamily
                    font.pixelSize: PairionStyle.fontSizeXs
                    width:          parent.width / 3
                }
                Text {
                    text:                "Mod"
                    color:               PairionStyle.dim
                    font.family:         PairionStyle.fontFamily
                    font.pixelSize:      PairionStyle.fontSizeXs
                    width:               parent.width / 3
                    horizontalAlignment: Text.AlignHCenter
                }
                Text {
                    text:                "Heavy"
                    color:               PairionStyle.dim
                    font.family:         PairionStyle.fontFamily
                    font.pixelSize:      PairionStyle.fontSizeXs
                    width:               parent.width / 3
                    horizontalAlignment: Text.AlignRight
                }
            }

            // UTC timestamp of the latest frame.
            Text {
                visible:        root.frames.length > 0
                text:           root.frames.length > 0
                                ? Qt.formatTime(
                                      new Date(root.frames[root.frames.length - 1].time * 1000),
                                      "HH:mm") + " UTC"
                                : ""
                color:          PairionStyle.dim
                opacity:        0.7
                font.family:    PairionStyle.fontFamily
                font.pixelSize: PairionStyle.fontSizeXs
            }
        }
    }

    // ── Loading indicator ─────────────────────────────────────────

    Text {
        anchors.centerIn: parent
        visible:          !root.dataLoaded
        text:             "Loading radar\u2026"
        color:            "#305070"
        font.family:      PairionStyle.fontFamily
        font.pixelSize:   PairionStyle.fontSizeLg
    }
}
