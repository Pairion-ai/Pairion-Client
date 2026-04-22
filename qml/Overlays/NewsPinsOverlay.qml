/**
 * @file NewsPinsOverlay.qml
 * @brief News pins overlay — geographic markers for live news events (stub).
 *
 * Stub implementation. Will display story pins at news event locations sourced from
 * SceneDataPush (modelId="news"). Calls background.latLonToScreen(lat, lon) to position
 * pins on the active background.
 */
import QtQuick

Item {
    id: root

    // ── OverlayBase contract ──────────────────────────────────────

    /** @brief Reference to the active background for coordinate mapping. */
    property var background: null
    /** @brief Parameters for this overlay from the most recent OverlayAdd command. */
    property var overlayParams: ({})
    /** @brief Model data keyed by modelId. */
    property var sceneData: ({})
    /** @brief Current agent state string from LayerManager. */
    property string hudState: "idle"

    /** @brief Request TTS narration. */
    signal requestSpeak(string text)
}
