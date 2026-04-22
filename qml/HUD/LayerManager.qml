/**
 * @file LayerManager.qml
 * @brief Composable layer system: manages the active background and overlay stack.
 *
 * Watches ConnectionState.activeBackgroundId and loads the corresponding background plugin
 * from qml/Backgrounds/<BackgroundId>Background.qml via Qt.createComponent(). Manages
 * crossfade and instant transitions between backgrounds using a dual-slot pattern identical
 * to the former SceneManager.
 *
 * Watches ConnectionState.activeOverlayIds and maintains a live overlay stack loaded from
 * qml/Overlays/<OverlayId>Overlay.qml. Overlays receive a reference to the active background
 * so they can call background.latLonToScreen(lat, lon) for coordinate mapping.
 *
 * Exposes currentBackground so PairionHUD can forward focusPin() calls to GlobeBackground.
 */
import QtQuick
import Pairion

Item {
    id: root

    // ── Public API ────────────────────────────────────────────────

    /**
     * @brief The currently active background instance, or null before first load.
     * PairionHUD uses this to forward focusPin() calls to GlobeBackground.
     */
    property var currentBackground: null

    // ── Private state ─────────────────────────────────────────────

    property var  _retiring:   null
    property bool _useA:       true
    /** @brief Live map from overlayId string to overlay Item instance. */
    property var  _overlayMap: ({})

    // ── Background slots ──────────────────────────────────────────
    // Two overlapping Item containers. The active slot fades in; the retiring
    // slot fades out. Using Item wrappers keeps anchors.fill inside each slot.

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

    // ── Overlay container — always rendered above both background slots ─

    Item {
        id: _overlayContainer
        anchors.fill: parent
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

    // Failsafe: destroy the retiring background after animation + 200 ms.
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

    // ── Live updates ──────────────────────────────────────────────

    Connections {
        target: ConnectionState

        function onActiveBackgroundIdChanged() {
            root._loadBackground(ConnectionState.activeBackgroundId,
                                 ConnectionState.sceneTransition)
        }
        function onActiveOverlayIdsChanged() {
            root._syncOverlays(ConnectionState.activeOverlayIds)
        }
        function onSceneDataChanged() {
            root._propagateSceneData()
        }
        function onOverlayParamsChanged() {
            root._propagateOverlayParams()
        }
        function onAgentStateChanged() {
            root._propagateHudState()
        }
        function onBackgroundParamsChanged() {
            if (root.currentBackground)
                root.currentBackground.backgroundParams = ConnectionState.backgroundParams
        }
    }

    // ── Lifecycle ─────────────────────────────────────────────────

    Component.onCompleted: {
        _loadBackground(ConnectionState.activeBackgroundId, "instant")
        _syncOverlays(ConnectionState.activeOverlayIds)
    }

    // ── Private helpers ───────────────────────────────────────────

    /**
     * @brief Converts a hyphenated identifier to PascalCase for QML filename lookup.
     * @param s e.g. "vfr" → "Vfr", "adsb-radar" → "AdsbRadar"
     * @return PascalCase string.
     */
    function _toPascalCase(s) {
        return s.split("-").map(function(part) {
            return part.charAt(0).toUpperCase() + part.slice(1)
        }).join("")
    }

    /**
     * @brief Load a background plugin and transition to it.
     * @param backgroundId Background identifier used to derive the QML file path.
     * @param transition   "crossfade" (default) or "instant".
     */
    function _loadBackground(backgroundId, transition) {
        var url  = "qrc:/qml/Backgrounds/" + _toPascalCase(backgroundId) + "Background.qml"
        var comp = Qt.createComponent(url)
        if (comp.status === Component.Error) {
            console.warn("LayerManager: failed to load background", url, "—", comp.errorString())
            return
        }

        // Alternate slots so a fading-out background is never clobbered.
        var activeSlot = _useA ? _slotA : _slotB
        var retireSlot = _useA ? _slotB : _slotA
        _useA = !_useA

        // Destroy any previous background in the target slot (from two transitions ago).
        if (activeSlot.children.length > 0) {
            activeSlot.children[0].destroy()
        }

        var bg = comp.createObject(activeSlot, {
            backgroundParams: ConnectionState.backgroundParams,
            sceneData:        ConnectionState.sceneData,
            hudState:         ConnectionState.agentState
        })

        if (!bg) {
            console.warn("LayerManager: createObject failed for background", url)
            return
        }

        bg.anchors.fill    = activeSlot
        root.currentBackground = bg

        // Propagate the new background reference to all active overlays.
        var ids = Object.keys(root._overlayMap)
        for (var i = 0; i < ids.length; i++) {
            root._overlayMap[ids[i]].background = bg
        }

        var instant = (transition === "instant" || !retireSlot.opacity)

        if (instant) {
            _fadeIn.stop()
            _fadeOut.stop()
            _cleanupTimer.stop()

            activeSlot.opacity = 1.0
            retireSlot.opacity = 0.0

            if (root._retiring) {
                if (root._retiring.children.length > 0)
                    root._retiring.children[0].destroy()
                root._retiring = null
            }
        } else {
            if (root._retiring) {
                _fadeOut.stop()
                _cleanupTimer.stop()
                if (root._retiring.children.length > 0)
                    root._retiring.children[0].destroy()
                root._retiring = null
            }

            root._retiring  = retireSlot
            _fadeIn.target  = activeSlot
            _fadeOut.target = retireSlot
            _fadeIn.restart()
            _fadeOut.restart()
            _cleanupTimer.restart()
        }
    }

    /**
     * @brief Synchronises the overlay stack to match the given ordered list of overlay IDs.
     * Adds overlays not yet present; removes overlays no longer in the list.
     * @param ids Ordered list of active overlay identifiers from ConnectionState.
     */
    function _syncOverlays(ids) {
        // Remove overlays no longer active.
        var currentIds = Object.keys(root._overlayMap)
        for (var i = 0; i < currentIds.length; i++) {
            var id = currentIds[i]
            var stillActive = false
            for (var j = 0; j < ids.length; j++) {
                if (ids[j] === id) { stillActive = true; break }
            }
            if (!stillActive) {
                root._overlayMap[id].destroy()
                delete root._overlayMap[id]
            }
        }

        // Add overlays not yet loaded.
        for (var k = 0; k < ids.length; k++) {
            var overlayId = ids[k]
            if (root._overlayMap[overlayId]) continue

            var url  = "qrc:/qml/Overlays/" + _toPascalCase(overlayId) + "Overlay.qml"
            var comp = Qt.createComponent(url)
            if (comp.status === Component.Error) {
                console.warn("LayerManager: failed to load overlay", url, "—", comp.errorString())
                continue
            }

            var params  = ConnectionState.overlayParams[overlayId] || {}
            var overlay = comp.createObject(_overlayContainer, {
                background:    root.currentBackground,
                overlayParams: params,
                sceneData:     ConnectionState.sceneData,
                hudState:      ConnectionState.agentState
            })

            if (!overlay) {
                console.warn("LayerManager: createObject failed for overlay", url)
                continue
            }

            overlay.anchors.fill = _overlayContainer
            root._overlayMap[overlayId] = overlay
        }
    }

    /** @brief Push updated sceneData to the background and all active overlays. */
    function _propagateSceneData() {
        var data = ConnectionState.sceneData
        if (root.currentBackground)
            root.currentBackground.sceneData = data
        var ids = Object.keys(root._overlayMap)
        for (var i = 0; i < ids.length; i++)
            root._overlayMap[ids[i]].sceneData = data
    }

    /** @brief Push updated overlayParams to each active overlay. */
    function _propagateOverlayParams() {
        var params = ConnectionState.overlayParams
        var ids    = Object.keys(root._overlayMap)
        for (var i = 0; i < ids.length; i++) {
            var p = params[ids[i]]
            if (p !== undefined) root._overlayMap[ids[i]].overlayParams = p
        }
    }

    /** @brief Push updated hudState to the background and all active overlays. */
    function _propagateHudState() {
        var state = ConnectionState.agentState
        if (root.currentBackground)
            root.currentBackground.hudState = state
        var ids = Object.keys(root._overlayMap)
        for (var i = 0; i < ids.length; i++)
            root._overlayMap[ids[i]].hudState = state
    }
}
