/**
 * @file DarkBackground.qml
 * @brief Dark background plugin — solid near-black fill for overlays that supply all visuals.
 *
 * Used when the active overlay (e.g. AdsbOverlay) provides its own background imagery and
 * no separate background scene is needed. Renders the same dark fill used by AdsbRadarScene
 * behind its chart image so the screen is never blank during overlay-only configurations.
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

    /** @brief Default coordinate bridge — DarkBackground is not geo-referenced. */
    function latLonToScreen(lat, lon) { return Qt.point(width * 0.5, height * 0.5) }

    /** @brief Request TTS narration. */
    signal requestSpeak(string text)

    // ── Dark fill ─────────────────────────────────────────────────

    Rectangle {
        anchors.fill: parent
        color: PairionStyle.darkBg
    }
}
