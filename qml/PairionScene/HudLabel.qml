/**
 * @file HudLabel.qml
 * @brief Reusable all-caps cyan header label for scene panels.
 *
 * Applies the standard Pairion HUD typography: Courier New, pixelSize 9,
 * letter-spacing 2, all-uppercase, cyan with reduced opacity.
 */
import QtQuick

Text {
    id: root

    color:               "#00b4ff"
    opacity:             0.7
    font.pixelSize:      9
    font.letterSpacing:  2
    font.family:         "Courier New"
    font.capitalization: Font.AllUppercase
}
