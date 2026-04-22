/**
 * @file SceneManager.qml
 * @brief Dynamic scene loader that replaces ContextBackground and HemisphereMap.
 *
 * Watches ConnectionState.activeSceneId and loads the corresponding scene plugin
 * from qml/Scenes/<sceneId>/<SceneId>Scene.qml via Qt.createComponent(). Manages
 * crossfade and instant transitions between scenes. Falls back to the "dashboard"
 * scene on any load error.
 *
 * Scene plugins are plain Items that declare the following properties
 * (see PairionScene/SceneBase.qml):
 *   property var    sceneData
 *   property var    sceneParams
 *   property string hudState
 *   signal requestScene(string sceneId, var params)
 *   signal requestSpeak(string text)
 *
 * The manager keeps currentScene pointing to the active scene instance so
 * PairionHUD can access scene-specific APIs (e.g. GlobeScene.activePinIndex).
 */
import QtQuick
import Pairion

Item {
    id: root

    // ── Public API ────────────────────────────────────────────────

    /**
     * @brief The currently active scene instance, or null before first load.
     * PairionHUD uses this to forward focusPin() calls to GlobeScene.
     */
    property var currentScene: null

    // ── Private state ─────────────────────────────────────────────

    property var _retiring: null   // scene currently fading out
    property bool _useA: true      // alternates to avoid clobbering active slot

    // ── Scene slots ───────────────────────────────────────────────
    // Two overlapping Item containers. The active slot fades in; the
    // retiring slot fades out. Using Item wrappers rather than bare
    // scenes so anchors.fill on the scene remains inside its slot.

    Item {
        id: _slotA
        anchors.fill: parent
        opacity: 0
    }

    Item {
        id: _slotB
        anchors.fill: parent
        opacity: 0
    }

    // ── Crossfade animations ──────────────────────────────────────

    NumberAnimation {
        id: _fadeIn
        property: "opacity"; from: 0; to: 1
        duration: 800; easing.type: Easing.InOutQuad
    }

    NumberAnimation {
        id: _fadeOut
        property: "opacity"; from: 1; to: 0
        duration: 800; easing.type: Easing.InOutQuad
        onFinished: {
            if (target && target === root._retiring) {
                if (root._retiring.children.length > 0) {
                    root._retiring.children[0].destroy()
                }
                root._retiring.opacity = 0
                root._retiring = null
            }
        }
    }

    // Failsafe: destroy retiring scene after animation duration + 200 ms.
    Timer {
        id: _cleanupTimer
        interval: 1000; repeat: false
        onTriggered: {
            if (root._retiring) {
                if (root._retiring.children.length > 0) {
                    root._retiring.children[0].destroy()
                }
                root._retiring.opacity = 0
                root._retiring = null
            }
        }
    }

    // ── Live data / state updates to the current scene ────────────

    Connections {
        target: ConnectionState

        function onActiveSceneIdChanged() {
            root._loadScene(ConnectionState.activeSceneId, ConnectionState.sceneTransition)
        }
        function onSceneDataChanged() {
            if (root.currentScene)
                root.currentScene.sceneData = ConnectionState.sceneData
        }
        function onSceneParamsChanged() {
            if (root.currentScene)
                root.currentScene.sceneParams = ConnectionState.sceneParams
        }
        function onAgentStateChanged() {
            if (root.currentScene)
                root.currentScene.hudState = ConnectionState.agentState
        }
    }

    // ── Lifecycle ─────────────────────────────────────────────────

    Component.onCompleted: _loadScene(ConnectionState.activeSceneId, "instant")

    // ── Private helpers ───────────────────────────────────────────

    /**
     * @brief Capitalises the first character of a string.
     * @param s Input string (e.g. "globe").
     * @return Capitalised string (e.g. "Globe").
     */
    function _capitalizeFirst(s) {
        return s.charAt(0).toUpperCase() + s.slice(1)
    }

    /**
     * @brief Load a scene plugin and transition to it.
     * @param sceneId    Scene identifier used to derive the QML file path.
     * @param transition "crossfade" (default) or "instant".
     */
    function _loadScene(sceneId, transition) {
        var url = "qrc:/qml/Scenes/" + sceneId + "/"
                + _capitalizeFirst(sceneId) + "Scene.qml"

        var comp = Qt.createComponent(url)
        if (comp.status === Component.Error) {
            console.warn("SceneManager: failed to load", url, "—", comp.errorString())
            if (sceneId !== "dashboard")
                _loadScene("dashboard", "instant")
            return
        }

        // Alternate active slot so we never clobber a fading-out scene.
        var activeSlot = _useA ? _slotA : _slotB
        var retireSlot = _useA ? _slotB : _slotA
        _useA = !_useA

        // Destroy any previous scene in the target slot now (from two transitions ago).
        if (activeSlot.children.length > 0) {
            activeSlot.children[0].destroy()
        }

        var scene = comp.createObject(activeSlot, {
            sceneData:   ConnectionState.sceneData,
            sceneParams: ConnectionState.sceneParams,
            hudState:    ConnectionState.agentState
        })

        if (!scene) {
            console.warn("SceneManager: createObject failed for", url)
            return
        }

        scene.anchors.fill = activeSlot
        root.currentScene  = scene

        var instant = (transition === "instant" || !retireSlot.opacity)

        if (instant) {
            // Abort any in-flight animations.
            _fadeIn.stop()
            _fadeOut.stop()
            _cleanupTimer.stop()

            activeSlot.opacity = 1.0
            retireSlot.opacity = 0.0

            // Destroy previous retiring scene if any.
            if (root._retiring) {
                if (root._retiring.children.length > 0)
                    root._retiring.children[0].destroy()
                root._retiring = null
            }
        } else {
            // Clean up any previously retiring scene so its Timer does not
            // destroy the newly assigned scene later.
            if (root._retiring) {
                _fadeOut.stop()
                _cleanupTimer.stop()
                if (root._retiring.children.length > 0)
                    root._retiring.children[0].destroy()
                root._retiring = null
            }

            root._retiring = retireSlot

            _fadeIn.target  = activeSlot
            _fadeOut.target = retireSlot
            _fadeIn.restart()
            _fadeOut.restart()
            _cleanupTimer.restart()
        }
    }
}
