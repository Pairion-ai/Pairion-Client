/**
 * @file SceneBase.qml
 * @brief Base component defining the contract for all PairionScene scene plugins.
 *
 * A scene plugin's root element should be SceneBase (or declare equivalent
 * properties) so the SceneManager can instantiate and configure it uniformly.
 *
 * SceneManager sets the four input properties on creation and updates
 * sceneData and sceneParams live as the server pushes changes.
 * A scene emits requestScene() to ask the SceneManager to switch to another
 * scene, and requestSpeak() to route TTS narration through the voice pipeline.
 */
import QtQuick

Item {
    id: root

    // ── Input properties (set and updated by SceneManager) ───────

    /** Width allocated to this scene — mirrors the SceneManager width. */
    property real sceneWidth: parent ? parent.width : 0

    /** Height allocated to this scene — mirrors the SceneManager height. */
    property real sceneHeight: parent ? parent.height : 0

    /**
     * @brief Model data keyed by modelId.
     * Populated via SceneDataPush messages. Example: sceneData["news"].
     */
    property var sceneData: ({})

    /**
     * @brief Parameters from the SceneChange command that activated this scene.
     */
    property var sceneParams: ({})

    /**
     * @brief Current agent state ("idle" | "listening" | "thinking" | "speaking" | "error").
     * Updated live by the SceneManager as ConnectionState.agentState changes.
     */
    property string hudState: "idle"

    // ── Output signals ────────────────────────────────────────────

    /**
     * @brief Request the SceneManager to switch to another scene.
     * @param sceneId Target scene identifier.
     * @param params  Optional parameters for the new scene.
     */
    signal requestScene(string sceneId, var params)

    /**
     * @brief Request that text be spoken via the voice pipeline.
     * @param text Narration text to synthesise.
     */
    signal requestSpeak(string text)
}
