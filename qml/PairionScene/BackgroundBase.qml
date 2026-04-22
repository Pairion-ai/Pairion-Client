/**
 * @file BackgroundBase.qml
 * @brief Base component defining the contract for all Pairion background layer plugins.
 *
 * A background plugin's root element should be BackgroundBase (or declare equivalent
 * properties) so the LayerManager can instantiate and configure it uniformly.
 *
 * LayerManager sets input properties on creation and updates them live as
 * ConnectionState changes. Backgrounds that support geographic coordinate mapping
 * (e.g. VfrBackground) override latLonToScreen() so overlay plugins can position
 * geographic elements correctly.
 */
import QtQuick

Item {
    id: root

    // ── Input properties (set and updated by LayerManager) ──────────

    /** @brief Parameters from the BackgroundChange command that activated this background. */
    property var backgroundParams: ({})

    /**
     * @brief Model data keyed by modelId.
     * Populated via SceneDataPush messages. Example: sceneData["adsb"].
     */
    property var sceneData: ({})

    /** @brief Current agent state ("idle" | "listening" | "thinking" | "speaking" | "error"). */
    property string hudState: "idle"

    // ── Coordinate bridge (override in geo-referenced backgrounds) ──

    /**
     * @brief Maps a geographic coordinate to a screen point.
     *
     * The default implementation returns the centre of the background, which is a safe
     * no-op for non-geographic backgrounds. Geo-referenced backgrounds (e.g. VfrBackground)
     * override this to return accurate pixel positions so overlay plugins can draw
     * geographic elements correctly.
     *
     * @param lat Latitude in decimal degrees.
     * @param lon Longitude in decimal degrees.
     * @return Qt.point(x, y) in screen pixels.
     */
    function latLonToScreen(lat, lon) {
        return Qt.point(width * 0.5, height * 0.5)
    }

    // ── Output signals ────────────────────────────────────────────

    /** @brief Request that text be spoken via the voice pipeline. */
    signal requestSpeak(string text)
}
