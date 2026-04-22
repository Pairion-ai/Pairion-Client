/**
 * @file PairionStyle.qml
 * @brief Singleton design-token registry for the PairionScene SDK.
 *
 * Import as: import PairionScene 1.0
 * All scenes and HUD panels source their colours, typography, and spacing from
 * this singleton so visual changes are propagated uniformly.
 */
pragma Singleton
import QtQuick

QtObject {
    // ── Colour palette ────────────────────────────────────────────
    readonly property color cyan:        "#00b4ff"
    readonly property color dim:         "#e0e8f0"
    readonly property color amber:       "#ff8800"
    readonly property color error:       "#ff3c3c"
    readonly property color panelBg:     "#0d1220"
    readonly property color darkBg:      "#070c18"
    readonly property color borderColor: "#00b4ff"

    // ── ADS-B radar aircraft colours ─────────────────────────────
    /** Colour for general aviation (GA) aircraft icons. */
    readonly property color gaColor:      "#00b4ff"
    /** Colour for airline/commercial aircraft icons. */
    readonly property color airlineColor: "#ff8800"

    // ── Typography ────────────────────────────────────────────────
    readonly property string fontFamily: "Courier New"
    readonly property int    fontSizeXs:  9
    readonly property int    fontSizeSm:  10
    readonly property int    fontSizeMd:  11
    readonly property int    fontSizeLg:  14

    // ── Spacing ───────────────────────────────────────────────────
    readonly property int    radiusSm:    4
    readonly property int    borderWidth: 1
}
