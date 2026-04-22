/**
 * @file VfrBackground.qml
 * @brief VFR sectional chart background — FAA chart image with geographic coordinate bridge.
 *
 * Renders an embedded FAA VFR Sectional chart image covering the ADS-B bounding box:
 *   lamin=33.398  lomin=-97.084  lamax=34.231  lomax=-96.080
 *
 * Overrides latLonToScreen() to convert geographic coordinates to screen pixels using
 * a simple linear projection. Overlay plugins (e.g. AdsbOverlay) call this function
 * to position geographic elements on top of the chart.
 *
 * Range rings at 5 nm, 10 nm, and 25 nm from the home location are drawn at low opacity
 * so the chart remains the dominant visual layer.
 *
 * To regenerate the sectional PNG after an FAA chart cycle (every 56 days), run:
 *   python3 scripts/process_vfr_sectional.py
 */
import QtQuick
import PairionScene 1.0

Item {
    id: root

    // ── BackgroundBase contract ───────────────────────────────────

    /** @brief Parameters from the BackgroundChange command that activated this background. */
    property var backgroundParams: ({})
    /** @brief Model data keyed by modelId. */
    property var sceneData: ({})
    /** @brief Current agent state string from LayerManager. */
    property string hudState: "idle"

    /** @brief Request TTS narration. */
    signal requestSpeak(string text)

    // ── Bounding box constants (must match application.yml pairion.data.adsb) ──

    /** @brief Southern boundary of the ADS-B coverage area (degrees). */
    readonly property real lamin:  33.398
    /** @brief Western boundary of the ADS-B coverage area (degrees). */
    readonly property real lomin: -97.084
    /** @brief Northern boundary of the ADS-B coverage area (degrees). */
    readonly property real lamax:  34.231
    /** @brief Eastern boundary of the ADS-B coverage area (degrees). */
    readonly property real lomax: -96.080

    /** @brief Home location latitude — centre of range rings. */
    readonly property real homeLat: 33.814427
    /** @brief Home location longitude — centre of range rings. */
    readonly property real homeLon: -96.582106

    // ── Coordinate bridge ─────────────────────────────────────────

    /**
     * @brief Maps a geographic coordinate to a screen point using linear projection
     * across the VFR chart bounding box.
     *
     * Coordinate formulas:
     *   screenX = (lon - lomin) / (lomax - lomin) * width
     *   screenY = (lamax - lat) / (lamax - lamin) * height
     *
     * @param lat Latitude in decimal degrees.
     * @param lon Longitude in decimal degrees.
     * @return Qt.point(x, y) in screen pixels.
     */
    function latLonToScreen(lat, lon) {
        return Qt.point(
            (lon - lomin) / (lomax - lomin) * width,
            (lamax - lat) / (lamax - lamin) * height
        )
    }

    // ── Layers ────────────────────────────────────────────────────

    // Dark base — shown during image load and behind transparent PNG edges.
    Rectangle {
        anchors.fill: parent
        color: PairionStyle.darkBg
    }

    // FAA VFR Sectional chart.
    Image {
        id: chartImage
        anchors.fill: parent
        source:   "qrc:/resources/adsb/vfr_sectional.png"
        fillMode: Image.Stretch
        smooth:   true
        visible:  false
        opacity: status === Image.Ready ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: 400 } }
    }

    // Range rings — drawn at 5 nm, 10 nm, 25 nm from home at low opacity.
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

            var pxPerNm = height / (root.lamax - root.lamin) / 60.0
            var home    = root.latLonToScreen(root.homeLat, root.homeLon)
            var cx      = home.x
            var cy      = home.y

            ctx.strokeStyle = "#00b4ff"
            ctx.lineWidth   = 1
            ctx.setLineDash([4, 6])
            ctx.font        = "9px 'Courier New'"
            ctx.fillStyle   = "#00b4ff"
            ctx.textAlign   = "left"

            var ringSets = [
                { nm: 5,  label: "5nm"  },
                { nm: 10, label: "10nm" },
                { nm: 25, label: "25nm" }
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
}
