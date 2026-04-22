/**
 * @file DashboardScene.qml
 * @brief Dashboard scene plugin — neutral dark background fallback.
 *
 * Serves as the default fallback when the server issues a SceneClear command
 * or when an unknown sceneId fails to load. Renders a plain deep-space dark
 * background with no animated elements.
 */
import QtQuick

Item {
    id: root

    // ── SceneBase contract ────────────────────────────────────────

    property var    sceneData:   ({})
    property var    sceneParams: ({})
    property string hudState:    "idle"

    signal requestScene(string sceneId, var params)
    signal requestSpeak(string text)

    // ── Background ────────────────────────────────────────────────

    Rectangle {
        anchors.fill: parent
        color: "#070c18"
    }
}
