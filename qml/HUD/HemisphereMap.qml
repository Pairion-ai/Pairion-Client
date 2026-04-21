/**
 * @file HemisphereMap.qml
 * @brief Dot-matrix hemisphere map with glowing amber news pins.
 *
 * Renders one hemisphere (western or eastern) as a grid of 2px dots on an
 * equirectangular projection. Land cells are painted cyan at low opacity;
 * ocean cells are transparent. News pins appear as pulsing amber dots with a
 * soft glow halo at their projected lat/lon positions.
 *
 * Projection: equirectangular — lon maps linearly to x, lat maps linearly to y.
 * Grid resolution: 2° per cell (180 cols × 90 rows for a full sphere).
 * Each hemisphere uses 90 columns (0°–180° for western = lon -180 to 0,
 * eastern = lon 0 to 180).
 *
 * Land mask: compact RLE string decoded at Component.onCompleted into a flat
 * boolean array. One character per 2°×2° cell: '1' = land, '0' = ocean.
 */

import QtQuick

Item {
    id: root

    // ── Public API ────────────────────────────────────────────────────────────

    /**
     * @brief Which hemisphere to show: "western" (-180..0) or "eastern" (0..180).
     */
    property string hemisphere: "western"

    /**
     * @brief News pin data. Each entry: { lat, lon, city, headline }.
     * Only pins whose longitude falls inside this hemisphere are rendered.
     */
    property var pins: []

    // ── Internal geometry constants ───────────────────────────────────────────

    // Full grid is 180 cols (2°/col) × 90 rows (2°/row)
    readonly property int gridCols: 90    // per hemisphere
    readonly property int gridRows: 90
    readonly property real dotDiam:    2
    readonly property real dotSpacing: 4.5   // center-to-center

    // Computed map canvas size (may be smaller than item if aspect differs)
    readonly property real mapW: gridCols * dotSpacing
    readonly property real mapH: gridRows * dotSpacing
    readonly property real offsetX: (width  - mapW) * 0.5
    readonly property real offsetY: (height - mapH) * 0.5

    // ── Land mask (2°×2° equirectangular, row-major lat 90→-90, full 360°) ──
    //
    // Encoded as a flat string of '0'/'1' characters, 180 cols × 90 rows = 16200 chars.
    // Row 0 = lat 90–88°N, row 89 = lat 88°S–90°S.
    // Col 0 = lon 180°W–178°W, col 179 = lon 178°E–180°E.
    //
    // This simplified mask is approximate — it captures major continental shapes
    // at sci-fi HUD fidelity, not cartographic accuracy.

    readonly property string landMaskRaw: (function() {
        // Helper: repeat a char n times
        function r(c, n) { var s = ""; for (var i = 0; i < n; i++) s += c; return s; }
        var O = "0", L = "1";
        var rows = [
            // row 0-4: Arctic (lat 90→80°N) — mostly ocean with Greenland/land
            r(O,55)+r(L,5)+r(O,10)+r(L,6)+r(O,10)+r(L,5)+r(O,89),   // 0
            r(O,50)+r(L,8)+r(O,8)+r(L,8)+r(O,8)+r(L,6)+r(O,92),     // 1
            r(O,48)+r(L,10)+r(O,6)+r(L,10)+r(O,6)+r(L,8)+r(O,92),   // 2
            r(O,46)+r(L,12)+r(O,5)+r(L,11)+r(O,5)+r(L,9)+r(O,92),   // 3
            r(O,44)+r(L,13)+r(O,4)+r(L,12)+r(O,4)+r(L,10)+r(O,93),  // 4
            // row 5-14: lat 80→60°N — Canada, Alaska, Russia, Scandinavia
            r(O,40)+r(L,15)+r(O,3)+r(L,13)+r(O,3)+r(L,14)+r(O,92),  // 5
            r(O,38)+r(L,17)+r(O,2)+r(L,14)+r(O,2)+r(L,15)+r(O,92),  // 6
            r(O,36)+r(L,19)+r(O,1)+r(L,15)+r(O,2)+r(L,16)+r(O,91),  // 7
            r(O,34)+r(L,20)+r(O,1)+r(L,16)+r(O,1)+r(L,17)+r(O,91),  // 8
            r(O,32)+r(L,22)+r(L,17)+r(L,18)+r(O,91),                 // 9
            r(O,30)+r(L,24)+r(L,18)+r(L,20)+r(O,88),                 // 10
            r(O,28)+r(L,25)+r(L,19)+r(L,21)+r(O,87),                 // 11
            r(O,26)+r(L,27)+r(L,20)+r(L,22)+r(O,85),                 // 12
            r(O,25)+r(L,28)+r(L,21)+r(L,23)+r(O,83),                 // 13
            r(O,24)+r(L,29)+r(L,22)+r(L,24)+r(O,81),                 // 14
            // row 15-24: lat 60→40°N — USA, Europe, China
            r(O,22)+r(L,30)+r(O,3)+r(L,20)+r(O,2)+r(L,25)+r(O,78),  // 15
            r(O,20)+r(L,31)+r(O,4)+r(L,19)+r(O,3)+r(L,26)+r(O,77),  // 16
            r(O,18)+r(L,32)+r(O,5)+r(L,18)+r(O,4)+r(L,27)+r(O,76),  // 17
            r(O,16)+r(L,33)+r(O,6)+r(L,17)+r(O,5)+r(L,28)+r(O,75),  // 18
            r(O,15)+r(L,33)+r(O,7)+r(L,16)+r(O,5)+r(L,29)+r(O,75),  // 19
            r(O,14)+r(L,34)+r(O,8)+r(L,15)+r(O,6)+r(L,28)+r(O,75),  // 20
            r(O,13)+r(L,34)+r(O,9)+r(L,14)+r(O,6)+r(L,28)+r(O,76),  // 21
            r(O,12)+r(L,35)+r(O,9)+r(L,14)+r(O,6)+r(L,27)+r(O,77),  // 22
            r(O,11)+r(L,35)+r(O,10)+r(L,13)+r(O,6)+r(L,27)+r(O,78), // 23
            r(O,10)+r(L,35)+r(O,11)+r(L,12)+r(O,7)+r(L,26)+r(O,79), // 24
            // row 25-34: lat 40→20°N — Mexico, N Africa, Middle East, S Asia
            r(O,10)+r(L,33)+r(O,12)+r(L,11)+r(O,8)+r(L,28)+r(O,78), // 25
            r(O,11)+r(L,30)+r(O,14)+r(L,10)+r(O,9)+r(L,28)+r(O,78), // 26
            r(O,12)+r(L,27)+r(O,15)+r(L,10)+r(O,9)+r(L,27)+r(O,80), // 27
            r(O,13)+r(L,24)+r(O,16)+r(L,10)+r(O,9)+r(L,26)+r(O,82), // 28
            r(O,14)+r(L,21)+r(O,17)+r(L,10)+r(O,9)+r(L,25)+r(O,84), // 29
            r(O,15)+r(L,18)+r(O,18)+r(L,11)+r(O,9)+r(L,24)+r(O,85), // 30
            r(O,16)+r(L,15)+r(O,19)+r(L,11)+r(O,9)+r(L,22)+r(O,88), // 31
            r(O,17)+r(L,12)+r(O,20)+r(L,10)+r(O,10)+r(L,20)+r(O,91),// 32
            r(O,18)+r(L,10)+r(O,21)+r(L,10)+r(O,10)+r(L,18)+r(O,93),// 33
            r(O,19)+r(L,8)+r(O,22)+r(L,10)+r(O,11)+r(L,16)+r(O,94), // 34
            // row 35-44: lat 20→0° — C America, C Africa, SE Asia
            r(O,20)+r(L,6)+r(O,23)+r(L,11)+r(O,11)+r(L,14)+r(O,95), // 35
            r(O,21)+r(L,5)+r(O,24)+r(L,12)+r(O,11)+r(L,12)+r(O,95), // 36
            r(O,22)+r(L,4)+r(O,25)+r(L,13)+r(O,11)+r(L,10)+r(O,95), // 37
            r(O,23)+r(L,3)+r(O,26)+r(L,14)+r(O,10)+r(L,9)+r(O,95),  // 38
            r(O,24)+r(L,3)+r(O,27)+r(L,15)+r(O,10)+r(L,8)+r(O,93),  // 39
            r(O,25)+r(L,3)+r(O,27)+r(L,15)+r(O,10)+r(L,9)+r(O,91),  // 40
            r(O,26)+r(L,3)+r(O,26)+r(L,15)+r(O,10)+r(L,10)+r(O,90), // 41
            r(O,27)+r(L,4)+r(O,25)+r(L,14)+r(O,10)+r(L,11)+r(O,89), // 42
            r(O,28)+r(L,5)+r(O,24)+r(L,13)+r(O,11)+r(L,10)+r(O,89), // 43
            r(O,29)+r(L,6)+r(O,23)+r(L,12)+r(O,12)+r(L,8)+r(O,90),  // 44
            // row 45-54: lat 0→20°S — S America, S Africa, Oceania
            r(O,30)+r(L,7)+r(O,22)+r(L,11)+r(O,12)+r(L,7)+r(O,91),  // 45
            r(O,31)+r(L,8)+r(O,21)+r(L,10)+r(O,12)+r(L,6)+r(O,92),  // 46
            r(O,32)+r(L,8)+r(O,21)+r(L,9)+r(O,11)+r(L,5)+r(O,94),   // 47
            r(O,33)+r(L,8)+r(O,22)+r(L,8)+r(O,10)+r(L,4)+r(O,95),   // 48
            r(O,34)+r(L,8)+r(O,22)+r(L,8)+r(O,9)+r(L,3)+r(O,96),    // 49
            r(O,35)+r(L,7)+r(O,23)+r(L,7)+r(O,8)+r(L,3)+r(O,97),    // 50
            r(O,36)+r(L,7)+r(O,23)+r(L,6)+r(O,8)+r(L,3)+r(O,97),    // 51
            r(O,37)+r(L,7)+r(O,23)+r(L,5)+r(O,9)+r(L,4)+r(O,95),    // 52
            r(O,38)+r(L,7)+r(O,22)+r(L,4)+r(O,10)+r(L,5)+r(O,94),   // 53
            r(O,39)+r(L,6)+r(O,22)+r(L,3)+r(O,11)+r(L,6)+r(O,93),   // 54
            // row 55-64: lat 20→40°S — S South America, S Africa, Australia
            r(O,40)+r(L,5)+r(O,23)+r(L,2)+r(O,12)+r(L,7)+r(O,91),   // 55
            r(O,41)+r(L,5)+r(O,58)+r(L,8)+r(O,88),                   // 56
            r(O,42)+r(L,4)+r(O,58)+r(L,8)+r(O,88),                   // 57
            r(O,43)+r(L,4)+r(O,57)+r(L,8)+r(O,88),                   // 58
            r(O,44)+r(L,4)+r(O,56)+r(L,8)+r(O,88),                   // 59
            r(O,45)+r(L,3)+r(O,55)+r(L,7)+r(O,90),                   // 60
            r(O,46)+r(L,3)+r(O,54)+r(L,6)+r(O,91),                   // 61
            r(O,47)+r(L,3)+r(O,53)+r(L,5)+r(O,92),                   // 62
            r(O,48)+r(L,2)+r(O,53)+r(L,4)+r(O,93),                   // 63
            r(O,49)+r(L,2)+r(O,52)+r(L,3)+r(O,94),                   // 64
            // row 65-74: lat 40→60°S — tip of S America, Antarctica approaches
            r(O,50)+r(L,2)+r(O,51)+r(L,2)+r(O,95),                   // 65
            r(O,51)+r(L,1)+r(O,128),                                   // 66
            r(O,52)+r(L,1)+r(O,127),                                   // 67
            r(O,180),                                                   // 68
            r(O,180),                                                   // 69
            r(O,180),                                                   // 70
            r(O,180),                                                   // 71
            r(O,180),                                                   // 72
            r(O,180),                                                   // 73
            r(O,180),                                                   // 74
            // row 75-89: lat 60→90°S — Antarctica
            r(O,35)+r(L,110)+r(O,35),                                  // 75
            r(O,25)+r(L,130)+r(O,25),                                  // 76
            r(O,18)+r(L,144)+r(O,18),                                  // 77
            r(O,12)+r(L,156)+r(O,12),                                  // 78
            r(O,8)+r(L,164)+r(O,8),                                    // 79
            r(O,5)+r(L,170)+r(O,5),                                    // 80
            r(O,3)+r(L,174)+r(O,3),                                    // 81
            r(O,2)+r(L,176)+r(O,2),                                    // 82
            r(O,1)+r(L,178)+r(O,1),                                    // 83
            r(L,180),                                                   // 84
            r(L,180),                                                   // 85
            r(L,180),                                                   // 86
            r(L,180),                                                   // 87
            r(L,180),                                                   // 88
            r(L,180)                                                    // 89
        ];
        return rows.join("");
    })()

    // Decoded land mask — flat array [row*180+col] = true/false
    property var landMask: []

    // ── News pin definitions ──────────────────────────────────────────────────

    /**
     * @brief All known news pins (both hemispheres). Filtered by hemisphere at render time.
     */
    readonly property var allPins: [
        // Western hemisphere (lon < 0)
        { lat:  40.7, lon:  -74.0, city: "New York",       headline: "Markets rally on Fed pause" },
        { lat:  34.0, lon: -118.2, city: "Los Angeles",    headline: "Tech layoffs continue" },
        { lat:  19.4, lon:  -99.1, city: "Mexico City",    headline: "Trade talks resume" },
        { lat: -23.5, lon:  -46.6, city: "São Paulo",      headline: "Amazon deforestation rate drops" },
        { lat: -34.6, lon:  -58.4, city: "Buenos Aires",   headline: "Argentina debt deal reached" },
        // Eastern hemisphere (lon >= 0)
        { lat:  51.5, lon:   -0.1, city: "London",         headline: "Bank of England holds rates" },
        { lat:  48.9, lon:    2.3, city: "Paris",          headline: "EU energy summit opens" },
        { lat:  55.8, lon:   37.6, city: "Moscow",         headline: "Arctic shipping route expands" },
        { lat:  35.7, lon:  139.7, city: "Tokyo",          headline: "Nikkei hits record high" },
        { lat: -33.9, lon:  151.2, city: "Sydney",         headline: "Australia GDP beats forecast" }
    ]

    // ── Land mask decode ──────────────────────────────────────────────────────

    Component.onCompleted: {
        var mask = []
        var raw  = landMaskRaw
        // Pad or truncate to exactly 180×90 = 16200 chars
        var expected = 180 * 90
        for (var i = 0; i < expected; i++) {
            mask.push(i < raw.length ? raw[i] === "1" : false)
        }
        landMask = mask
        landCanvas.requestPaint()
    }

    // ── Helpers ───────────────────────────────────────────────────────────────

    /**
     * @brief Convert lat/lon to pixel position on this hemisphere's canvas.
     * Returns {x, y} relative to the map canvas origin (top-left of dot grid).
     * Returns null if the pin is outside this hemisphere.
     */
    function latLonToXY(lat, lon) {
        var isWest = (hemisphere === "western")
        // Western: lon -180..0  →  col 0..89
        // Eastern: lon 0..180   →  col 0..89
        var lonMin = isWest ? -180 : 0
        var lonMax = isWest ?    0 : 180
        if (lon < lonMin || lon > lonMax) return null

        var col = (lon - lonMin) / (lonMax - lonMin) * (gridCols - 1)
        var row = (90 - lat) / 180 * (gridRows - 1)
        return {
            x: offsetX + col * dotSpacing + dotSpacing * 0.5,
            y: offsetY + row * dotSpacing + dotSpacing * 0.5
        }
    }

    /**
     * @brief Check whether a grid cell is land for this hemisphere.
     * col is the LOCAL hemisphere column (0–89), row is global (0–89).
     */
    function isLand(localCol, row) {
        if (landMask.length === 0) return false
        // Map local col → global col
        // Western: local 0 = global col 0 (lon -180), local 89 = global col 89 (lon 0)
        // Eastern: local 0 = global col 90 (lon 0),   local 89 = global col 179 (lon 180)
        var globalCol = (hemisphere === "western") ? localCol : (localCol + 90)
        var idx = row * 180 + globalCol
        return (idx >= 0 && idx < landMask.length) ? landMask[idx] : false
    }

    // ── Dot canvas ────────────────────────────────────────────────────────────

    Canvas {
        id: landCanvas
        anchors.fill: parent

        onPaint: {
            if (root.landMask.length === 0) return
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            ctx.fillStyle = "rgba(0, 180, 255, 0.30)"

            for (var row = 0; row < root.gridRows; row++) {
                for (var col = 0; col < root.gridCols; col++) {
                    if (!root.isLand(col, row)) continue
                    var cx = root.offsetX + col * root.dotSpacing + root.dotSpacing * 0.5
                    var cy = root.offsetY + row * root.dotSpacing + root.dotSpacing * 0.5
                    ctx.beginPath()
                    ctx.arc(cx, cy, root.dotDiam * 0.5, 0, Math.PI * 2)
                    ctx.fill()
                }
            }
        }
    }

    // ── News pins ─────────────────────────────────────────────────────────────

    Repeater {
        model: root.allPins.length

        delegate: Item {
            id: pinDelegate

            required property int index

            readonly property var pinData: root.allPins[index]
            readonly property var pos: root.latLonToXY(pinData.lat, pinData.lon)
            readonly property bool visible2: pos !== null

            visible: visible2
            x: visible2 ? pos.x - 8 : 0
            y: visible2 ? pos.y - 8 : 0
            width:  16
            height: 16

            // Glow halo — wider faded circle behind
            Rectangle {
                anchors.centerIn: parent
                width:  14
                height: 14
                radius: 7
                color:  "transparent"
                border.color: "#ff8800"
                border.width: 2
                opacity: 0.30
            }

            // Pin dot
            Rectangle {
                id: pinDot
                anchors.centerIn: parent
                width:  6
                height: 6
                radius: 3
                color:  "#ff8800"

                // Pulse: opacity 0.70 → 1.0 → 0.70, 2 s cycle, staggered
                SequentialAnimation on opacity {
                    loops: Animation.Infinite
                    // Stagger each pin by index * 200 ms
                    PauseAnimation   { duration: (pinDelegate.index * 200) % 2000 }
                    NumberAnimation  { from: 0.70; to: 1.00; duration: 1000; easing.type: Easing.InOutSine }
                    NumberAnimation  { from: 1.00; to: 0.70; duration: 1000; easing.type: Easing.InOutSine }
                }
            }
        }
    }
}
