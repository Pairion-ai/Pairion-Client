/**
 * @file OsmBackground.qml
 * @brief OpenStreetMap dark-styled raster tile background.
 *
 * Renders CartoDB dark_matter Web Mercator tiles to fill the viewport.
 * Tiles are loaded asynchronously, cached by Qt's image cache, and laid out
 * with a one-tile margin on all four sides so pan/zoom reveals no blank edges.
 *
 * Implements latLonToScreen() using standard Slippy Map math so geographic
 * overlays (ADS-B, weather radar, markers, routes) position their elements
 * correctly on the street map — identical to any other geographic background.
 *
 * Tile source (free, no API key required):
 *   https://basemaps.cartocdn.com/dark_all/{z}/{x}/{y}.png
 *
 * Parameters read from backgroundParams:
 * - center_lat: latitude of viewport centre (default: 33.814427)
 * - center_lon: longitude of viewport centre (default: -96.582106)
 * - zoom: tile zoom level 0–19, floored to integer (default: 10)
 */
import QtQuick
import PairionScene 1.0

Item {
    id: root

    // ── BackgroundBase contract ───────────────────────────────────

    /** @brief Parameters from the BackgroundChange command that activated this background. */
    property var backgroundParams: ({})

    /** @brief Model data keyed by modelId (passed through from LayerManager). */
    property var sceneData: ({})

    /** @brief Current agent state string from LayerManager. */
    property string hudState: "idle"

    /** @brief Request TTS narration. */
    signal requestSpeak(string text)

    // ── Tile state ────────────────────────────────────────────────

    /**
     * @brief Ordered list of tile descriptor objects for the current viewport.
     *
     * Each element is a plain JS object: { url: string, x: real, y: real }
     * where (x, y) is the screen-pixel position of the tile's top-left corner.
     */
    property var tileList: []

    // ── Lifecycle ─────────────────────────────────────────────────

    Component.onCompleted:     _rebuildTiles()
    onBackgroundParamsChanged: _rebuildTiles()
    onWidthChanged:            _rebuildTiles()
    onHeightChanged:           _rebuildTiles()

    // ── Coordinate bridge (geographic overlay interface) ──────────

    /**
     * @brief Maps a geographic coordinate to a screen pixel using Web Mercator projection.
     *
     * Formula (Slippy Map / OSM standard):
     *   worldSize = 256 * 2^z
     *   pixelX    = (lon + 180) / 360 * worldSize
     *   pixelY    = (1 - log(tan(lat·π/180) + 1/cos(lat·π/180)) / π) / 2 * worldSize
     *   screenX   = (pixelX - centrePixelX) + width / 2
     *   screenY   = (pixelY - centrePixelY) + height / 2
     *
     * The result aligns exactly with the rendered tile grid so overlay elements
     * land on the correct streets and landmarks.
     *
     * @param lat Latitude in decimal degrees.
     * @param lon Longitude in decimal degrees.
     * @return Qt.point(x, y) in screen pixels.
     */
    function latLonToScreen(lat, lon) {
        var z         = Math.floor(backgroundParams.zoom || 10)
        var worldSize = 256 * Math.pow(2, z)

        var latRad = lat * Math.PI / 180
        var px     = (lon + 180) / 360 * worldSize
        var py     = (1 - Math.log(Math.tan(latRad) + 1 / Math.cos(latRad)) / Math.PI) / 2 * worldSize

        var cLat    = backgroundParams.center_lat || 33.814427
        var cLon    = backgroundParams.center_lon || -96.582106
        var cLatRad = cLat * Math.PI / 180
        var cpx     = (cLon + 180) / 360 * worldSize
        var cpy     = (1 - Math.log(Math.tan(cLatRad) + 1 / Math.cos(cLatRad)) / Math.PI) / 2 * worldSize

        return Qt.point(
            (px  - cpx) + width  * 0.5,
            (py  - cpy) + height * 0.5
        )
    }

    // ── Private helpers ───────────────────────────────────────────

    /**
     * @brief Recomputes the tile grid for the current viewport, centre, and zoom level.
     *
     * Determines which Slippy Map tiles cover the viewport (with a one-tile margin
     * on all sides to prevent visible gaps), computes each tile's screen position,
     * and writes the result into tileList. The Repeater delegate binds to tileList
     * and creates/destroys Image elements as the array changes.
     *
     * Uses the actual item dimensions when available; falls back to 2560×1440 before
     * the component receives its first geometry update.
     */
    function _rebuildTiles() {
        var vw = width  > 0 ? width  : 2560
        var vh = height > 0 ? height : 1440

        var z        = Math.floor(backgroundParams.zoom || 10)
        var numTiles = Math.pow(2, z)
        var wsz      = 256 * numTiles   // total world size in pixels

        var cLat    = backgroundParams.center_lat || 33.814427
        var cLon    = backgroundParams.center_lon || -96.582106
        var cLatRad = cLat * Math.PI / 180
        var cpx     = (cLon + 180) / 360 * wsz
        var cpy     = (1 - Math.log(Math.tan(cLatRad) + 1 / Math.cos(cLatRad)) / Math.PI) / 2 * wsz

        // Fractional tile index at the viewport centre.
        var ctxf = cpx / 256
        var ctyf = cpy / 256

        // Tile count from centre to viewport edge, plus one-tile margin.
        var halfX = Math.ceil(vw / 256 / 2) + 1
        var halfY = Math.ceil(vh / 256 / 2) + 1

        var minTX = Math.floor(ctxf) - halfX
        var maxTX = Math.floor(ctxf) + halfX
        var minTY = Math.floor(ctyf) - halfY
        var maxTY = Math.floor(ctyf) + halfY

        var tiles = []
        for (var ty = minTY; ty <= maxTY; ty++) {
            // Skip tiles outside the valid latitude band.
            if (ty < 0 || ty >= numTiles) continue

            for (var tx = minTX; tx <= maxTX; tx++) {
                // Wrap X for seamless horizontal tiling across the antimeridian.
                var wrappedTX = ((tx % numTiles) + numTiles) % numTiles

                // World-pixel position of the tile's top-left corner.
                var twx = tx * 256
                var twy = ty * 256

                // Screen position: offset from world centre, then shift to viewport centre.
                var sx = twx - cpx + vw * 0.5
                var sy = twy - cpy + vh * 0.5

                tiles.push({
                    url: "https://basemaps.cartocdn.com/dark_all/"
                         + z + "/" + wrappedTX + "/" + ty + ".png",
                    x: Math.round(sx),
                    y: Math.round(sy)
                })
            }
        }

        tileList = tiles
    }

    // ── Visual layers ─────────────────────────────────────────────

    // Dark fill shown behind and between tiles while they load.
    Rectangle {
        anchors.fill: parent
        color: PairionStyle.darkBg
    }

    // Tile grid — one Image per entry in tileList.
    Repeater {
        model: root.tileList

        /**
         * @brief Single raster tile delegate.
         *
         * Loads the tile URL asynchronously; Qt caches the decoded image in memory
         * so subsequent reuse of the same tile (e.g. after a param change that
         * returns to the same zoom/position) avoids a network round-trip.
         */
        delegate: Image {
            required property var modelData

            x:            modelData.x
            y:            modelData.y
            width:        256
            height:       256
            source:       modelData.url
            cache:        true
            asynchronous: true
            smooth:       true
            fillMode:     Image.Stretch
        }
    }
}
