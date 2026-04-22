/**
 * @file OverlayBase.qml
 * @brief Base component defining the contract for all Pairion overlay layer plugins.
 *
 * Overlay plugins are stacked above the active background inside the LayerManager.
 * The LayerManager sets the background property so overlays can call
 * background.latLonToScreen(lat, lon) to convert geographic coordinates to screen pixels.
 *
 * Guard every latLonToScreen call: if (!background) return.
 */
import QtQuick

Item {
    id: root

    // ── Input properties (set and updated by LayerManager) ──────────

    /**
     * @brief Reference to the currently active background instance.
     * Overlays call background.latLonToScreen(lat, lon) for coordinate mapping.
     */
    property var background: null

    /** @brief Parameters for this overlay from the most recent OverlayAdd command. */
    property var overlayParams: ({})

    /**
     * @brief Model data keyed by modelId.
     * Populated via SceneDataPush messages. Example: sceneData["adsb"].
     */
    property var sceneData: ({})

    /** @brief Current agent state ("idle" | "listening" | "thinking" | "speaking" | "error"). */
    property string hudState: "idle"

    // ── Output signals ────────────────────────────────────────────

    /** @brief Request that text be spoken via the voice pipeline. */
    signal requestSpeak(string text)
}
