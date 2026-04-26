/**
 * @file WeatherCurrentOverlay.qml
 * @brief Current weather conditions panel — non-geographic corner-anchored overlay.
 *
 * Displays current weather data received via SceneDataPush (modelId="weather_current").
 * Rendered as a semi-transparent HUD panel anchored to the bottom-left corner.
 * Works on any background including non-geographic ones (space, dark).
 *
 * Data fields (from WeatherCurrentSnapshot):
 *   city, temperatureF, feelsLikeF, highF, lowF, humidity,
 *   windSpeedMph, windDirectionDeg, conditions, precipitationIn, pressureMb
 *
 * Opacity: 0.5 when hudState="idle" (ambient mode); 1.0 otherwise.
 */
import QtQuick
import PairionScene 1.0

Item {
    id: root

    // ── OverlayBase contract ──────────────────────────────────────

    /** @brief Reference to the active background (unused — non-geographic overlay). */
    property var background: null

    /** @brief Parameters from the most recent OverlayAdd command. */
    property var overlayParams: ({})

    /** @brief Model data keyed by modelId (sceneData["weather_current"] = snapshot). */
    property var sceneData: ({})

    /** @brief Current agent state string from LayerManager. */
    property string hudState: "idle"

    /** @brief Request TTS narration (unused by this overlay). */
    signal requestSpeak(string text)

    // ── Derived weather data ──────────────────────────────────────

    /**
     * @brief Live weather snapshot from SceneDataPush (modelId="weather_current").
     * Null when no data has been pushed yet.
     */
    readonly property var wx: sceneData["weather_current"] || null

    // ── Visibility and opacity ────────────────────────────────────

    /** @brief Reduce to 0.5 opacity when idle so the panel is ambient rather than intrusive. */
    opacity: (hudState === "idle") ? 0.5 : 1.0
    Behavior on opacity { NumberAnimation { duration: 400 } }

    // ── Panel ─────────────────────────────────────────────────────

    Rectangle {
        id: panel
        anchors.left:   parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 20
        width:  panelCol.implicitWidth  + 20
        height: panelCol.implicitHeight + 16
        color:        Qt.rgba(0.05, 0.07, 0.13, 0.88)
        border.color: PairionStyle.borderColor
        border.width: PairionStyle.borderWidth
        radius:       PairionStyle.radiusSm

        Column {
            id: panelCol
            anchors {
                left:    parent.left
                right:   parent.right
                top:     parent.top
                margins: 10
            }
            spacing: 5

            // ── Loading / placeholder ─────────────────────────────

            Text {
                visible:        root.wx === null
                text:           "Loading\u2026"
                color:          PairionStyle.dim
                font.family:    PairionStyle.fontFamily
                font.pixelSize: PairionStyle.fontSizeLg
                opacity:        0.5
            }

            // ── City name ─────────────────────────────────────────

            Text {
                visible:        root.wx !== null
                text:           root.wx ? (root.wx.city || "\u2014") : ""
                color:          PairionStyle.cyan
                font.family:    PairionStyle.fontFamily
                font.pixelSize: PairionStyle.fontSizeLg
                font.bold:      true
            }

            // ── Temperature (large) ───────────────────────────────

            Text {
                visible:        root.wx !== null
                text:           root.wx ? Math.round(root.wx.temperatureF) + "\u00b0F" : ""
                color:          PairionStyle.dim
                font.family:    PairionStyle.fontFamily
                font.pixelSize: 28
                font.bold:      true
            }

            // ── Conditions ────────────────────────────────────────

            Text {
                visible:        root.wx !== null
                text:           root.wx ? (root.wx.conditions || "\u2014") : ""
                color:          PairionStyle.dim
                font.family:    PairionStyle.fontFamily
                font.pixelSize: PairionStyle.fontSizeMd
            }

            // ── Feels like ────────────────────────────────────────

            Text {
                visible:        root.wx !== null
                text:           root.wx ? "Feels like " + Math.round(root.wx.feelsLikeF) + "\u00b0F" : ""
                color:          PairionStyle.dim
                font.family:    PairionStyle.fontFamily
                font.pixelSize: PairionStyle.fontSizeSm
                opacity:        0.85
            }

            // ── High / Low ────────────────────────────────────────

            Text {
                visible:        root.wx !== null
                text:           root.wx
                                ? "H\u00a0" + Math.round(root.wx.highF) + "\u00b0  L\u00a0" + Math.round(root.wx.lowF) + "\u00b0"
                                : ""
                color:          PairionStyle.dim
                font.family:    PairionStyle.fontFamily
                font.pixelSize: PairionStyle.fontSizeSm
                opacity:        0.80
            }

            // ── Divider ───────────────────────────────────────────

            Rectangle {
                visible: root.wx !== null
                width:   panelCol.width
                height:  1
                color:   PairionStyle.borderColor
                opacity: 0.3
            }

            // ── Humidity ──────────────────────────────────────────

            Text {
                visible:        root.wx !== null
                text:           root.wx ? "Humidity   " + root.wx.humidity + "%" : ""
                color:          PairionStyle.dim
                font.family:    PairionStyle.fontFamily
                font.pixelSize: PairionStyle.fontSizeSm
            }

            // ── Wind ─────────────────────────────────────────────

            Text {
                visible:        root.wx !== null
                text:           root.wx
                                ? "Wind   " + Math.round(root.wx.windSpeedMph) + " mph  "
                                  + _windDir(root.wx.windDirectionDeg)
                                : ""
                color:          PairionStyle.dim
                font.family:    PairionStyle.fontFamily
                font.pixelSize: PairionStyle.fontSizeSm
            }

            // ── Precipitation ─────────────────────────────────────

            Text {
                visible:        root.wx !== null && root.wx.precipitationIn > 0
                text:           root.wx ? "Precip   " + root.wx.precipitationIn.toFixed(2) + " in" : ""
                color:          PairionStyle.dim
                font.family:    PairionStyle.fontFamily
                font.pixelSize: PairionStyle.fontSizeSm
            }

            // ── Pressure ──────────────────────────────────────────

            Text {
                visible:        root.wx !== null
                text:           root.wx ? "Pressure   " + Math.round(root.wx.pressureMb) + " mb" : ""
                color:          PairionStyle.dim
                font.family:    PairionStyle.fontFamily
                font.pixelSize: PairionStyle.fontSizeXs
                opacity:        0.70
            }
        }
    }

    // ── Compass direction helper ──────────────────────────────────

    /**
     * @brief Converts a wind direction in degrees to a compass bearing abbreviation.
     * @param deg Wind direction in degrees clockwise from true north (0–359).
     * @return Compass abbreviation string: N, NE, E, SE, S, SW, W, or NW.
     */
    function _windDir(deg) {
        if (typeof deg !== "number") return ""
        var dirs = ["N","NE","E","SE","S","SW","W","NW"]
        return dirs[Math.round(deg / 45) % 8]
    }
}
