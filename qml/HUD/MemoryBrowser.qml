/**
 * @file MemoryBrowser.qml
 * @brief Right-side slide-in panel displaying memory episodes, turns, and preferences.
 *
 * Fills the parent Item. The panel slides in from the right edge when memoryVisible
 * is set to true. Driven by the MemoryBrowserModel singleton registered in main.cpp.
 * Auto-refreshes on open. Use the M keyboard shortcut (wired in Main.qml) to toggle.
 */
import QtQuick
import QtQuick.Controls
import PairionScene
import Pairion

Item {
    id: root

    /// Set true to slide the panel in; false to slide it out.
    property bool memoryVisible: false

    readonly property int panelWidth: Math.max(400, Math.floor(parent.width * 0.30))

    // ── Dim background scrim ─────────────────────────────────────────────────
    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: root.memoryVisible ? 0.45 : 0.0
        Behavior on opacity { NumberAnimation { duration: 220 } }
    }

    // ── Slide panel ──────────────────────────────────────────────────────────
    Rectangle {
        id: panel
        width: root.panelWidth
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        x: root.memoryVisible ? parent.width - root.panelWidth : parent.width

        Behavior on x { NumberAnimation { duration: 280; easing.type: Easing.OutCubic } }

        color: PairionStyle.panelBg
        border.color: PairionStyle.cyan
        border.width: 1

        // ── Header ───────────────────────────────────────────────────────────
        Rectangle {
            id: header
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 44
            color: "transparent"
            border.color: PairionStyle.cyan
            border.width: 0

            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: PairionStyle.cyan
                opacity: 0.25
            }

            Row {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 14
                spacing: 0

                Text {
                    text: "\u25C4 "
                    color: PairionStyle.cyan
                    font.pixelSize: PairionStyle.fontSizeSm
                    font.family: PairionStyle.fontFamily
                    opacity: 0.6

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.memoryVisible = false
                    }
                }

                Text {
                    text: "MEMORY BROWSER"
                    color: PairionStyle.cyan
                    font.pixelSize: PairionStyle.fontSizeSm
                    font.letterSpacing: 1.5
                    font.family: PairionStyle.fontFamily
                }
            }

            // Refresh button
            Text {
                text: "\u21BB"
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: 14
                color: PairionStyle.cyan
                font.pixelSize: PairionStyle.fontSizeMd
                font.family: PairionStyle.fontFamily
                opacity: MemoryBrowserModel.loading ? 0.3 : 0.7

                MouseArea {
                    anchors.fill: parent
                    enabled: !MemoryBrowserModel.loading
                    cursorShape: Qt.PointingHandCursor
                    onClicked: MemoryBrowserModel.refresh()
                }
            }
        }

        // ── Tab bar ───────────────────────────────────────────────────────────
        Row {
            id: tabBar
            anchors.top: header.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 32

            property int activeTab: 0  // 0 = Episodes, 1 = Preferences

            Repeater {
                model: ["EPISODES", "PREFERENCES"]

                Rectangle {
                    width: panel.width / 2
                    height: tabBar.height
                    color: "transparent"

                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: tabBar.activeTab === index ? 2 : 1
                        color: tabBar.activeTab === index ? PairionStyle.cyan
                                                          : PairionStyle.cyan
                        opacity: tabBar.activeTab === index ? 0.9 : 0.2
                    }

                    Text {
                        anchors.centerIn: parent
                        text: modelData
                        color: PairionStyle.cyan
                        opacity: tabBar.activeTab === index ? 1.0 : 0.4
                        font.pixelSize: PairionStyle.fontSizeXs
                        font.letterSpacing: 1.2
                        font.family: PairionStyle.fontFamily
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            tabBar.activeTab = index
                            if (index === 0) MemoryBrowserModel.clearSelection()
                        }
                    }
                }
            }
        }

        // ── Content area ──────────────────────────────────────────────────────
        Item {
            id: contentArea
            anchors.top: tabBar.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.topMargin: 4

            // Loading state
            Text {
                anchors.centerIn: parent
                text: "LOADING..."
                color: PairionStyle.cyan
                opacity: 0.5
                font.pixelSize: PairionStyle.fontSizeSm
                font.letterSpacing: 1.5
                font.family: PairionStyle.fontFamily
                visible: MemoryBrowserModel.loading
            }

            // Error state
            Item {
                anchors.fill: parent
                anchors.margins: 16
                visible: !MemoryBrowserModel.loading && MemoryBrowserModel.errorMessage !== ""

                Text {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    text: "ERROR"
                    color: PairionStyle.error
                    font.pixelSize: PairionStyle.fontSizeXs
                    font.letterSpacing: 1.5
                    font.family: PairionStyle.fontFamily
                    opacity: 0.7
                }

                Text {
                    anchors.top: parent.top
                    anchors.topMargin: 18
                    anchors.left: parent.left
                    anchors.right: parent.right
                    text: MemoryBrowserModel.errorMessage
                    color: PairionStyle.error
                    font.pixelSize: PairionStyle.fontSizeXs
                    font.family: PairionStyle.fontFamily
                    wrapMode: Text.WordWrap
                    opacity: 0.8
                }
            }

            // ── Episodes tab ─────────────────────────────────────────────────
            Item {
                anchors.fill: parent
                visible: !MemoryBrowserModel.loading &&
                         MemoryBrowserModel.errorMessage === "" &&
                         tabBar.activeTab === 0

                // Episode list (no selection yet)
                Item {
                    anchors.fill: parent
                    visible: MemoryBrowserModel.selectedEpisodeId === ""

                    // Empty state
                    Text {
                        anchors.centerIn: parent
                        text: "NO MEMORY"
                        color: PairionStyle.cyan
                        opacity: 0.3
                        font.pixelSize: PairionStyle.fontSizeSm
                        font.letterSpacing: 1.5
                        font.family: PairionStyle.fontFamily
                        visible: !MemoryBrowserModel.hasMemory
                    }

                    ScrollView {
                        anchors.fill: parent
                        clip: true
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                        visible: MemoryBrowserModel.hasMemory

                        Column {
                            width: panel.width
                            spacing: 0

                            Repeater {
                                model: MemoryBrowserModel.episodes

                                Rectangle {
                                    width: panel.width
                                    height: 56
                                    color: episodeMouse.containsMouse ? "#1a2540" : "transparent"

                                    Rectangle {
                                        anchors.bottom: parent.bottom
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.leftMargin: 12
                                        anchors.rightMargin: 12
                                        height: 1
                                        color: PairionStyle.cyan
                                        opacity: 0.1
                                    }

                                    Column {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        anchors.leftMargin: 12
                                        anchors.rightMargin: 12
                                        spacing: 3

                                        Text {
                                            text: modelData.title || modelData.id
                                            color: PairionStyle.dim
                                            font.pixelSize: PairionStyle.fontSizeSm
                                            font.family: PairionStyle.fontFamily
                                            elide: Text.ElideRight
                                            width: parent.width
                                        }

                                        Row {
                                            spacing: 10

                                            Text {
                                                text: modelData.startTime
                                                color: PairionStyle.cyan
                                                opacity: 0.5
                                                font.pixelSize: PairionStyle.fontSizeXs
                                                font.family: PairionStyle.fontFamily
                                            }

                                            Text {
                                                text: modelData.turnCount + " turns"
                                                color: PairionStyle.cyan
                                                opacity: 0.4
                                                font.pixelSize: PairionStyle.fontSizeXs
                                                font.family: PairionStyle.fontFamily
                                            }
                                        }
                                    }

                                    MouseArea {
                                        id: episodeMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: MemoryBrowserModel.selectEpisode(modelData.id)
                                    }
                                }
                            }
                        }
                    }
                }

                // Turn view (episode selected)
                Item {
                    anchors.fill: parent
                    visible: MemoryBrowserModel.selectedEpisodeId !== ""

                    // Back button
                    Rectangle {
                        id: backBar
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: 32
                        color: "transparent"

                        Rectangle {
                            anchors.bottom: parent.bottom
                            anchors.left: parent.left
                            anchors.right: parent.right
                            height: 1
                            color: PairionStyle.cyan
                            opacity: 0.15
                        }

                        Text {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 12
                            text: "\u25C4 BACK"
                            color: PairionStyle.cyan
                            opacity: 0.7
                            font.pixelSize: PairionStyle.fontSizeXs
                            font.letterSpacing: 1.2
                            font.family: PairionStyle.fontFamily

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: MemoryBrowserModel.clearSelection()
                            }
                        }
                    }

                    // Turn list
                    ScrollView {
                        anchors.top: backBar.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.topMargin: 4
                        clip: true
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                        Column {
                            width: panel.width - 2
                            spacing: 6
                            topPadding: 6
                            bottomPadding: 10

                            Repeater {
                                model: MemoryBrowserModel.selectedEpisodeTurns

                                Item {
                                    width: panel.width - 2
                                    height: bubble.height + 8

                                    readonly property bool isUser: modelData.role === "user"

                                    Rectangle {
                                        id: bubble
                                        anchors.top: parent.top
                                        anchors.topMargin: 4
                                        anchors.left: parent.isUser ? undefined : parent.left
                                        anchors.right: parent.isUser ? parent.right : undefined
                                        anchors.leftMargin: parent.isUser ? 0 : 10
                                        anchors.rightMargin: parent.isUser ? 10 : 0
                                        width: Math.min(turnText.implicitWidth + 16,
                                                        panel.width * 0.82)
                                        height: turnText.height + 16
                                        radius: PairionStyle.radiusSm
                                        color: parent.isUser ? "#0f1e38" : "#12201a"
                                        border.color: parent.isUser ? PairionStyle.cyan
                                                                    : "#00cc88"
                                        border.width: 1

                                        Column {
                                            anchors.left: parent.left
                                            anchors.right: parent.right
                                            anchors.top: parent.top
                                            anchors.margins: 8
                                            spacing: 4

                                            Text {
                                                text: modelData.role.toUpperCase()
                                                color: parent.parent.parent.isUser
                                                           ? PairionStyle.cyan : "#00cc88"
                                                opacity: 0.6
                                                font.pixelSize: 8
                                                font.letterSpacing: 1.2
                                                font.family: PairionStyle.fontFamily
                                            }

                                            Text {
                                                id: turnText
                                                width: bubble.width - 16
                                                text: modelData.content
                                                color: PairionStyle.dim
                                                font.pixelSize: PairionStyle.fontSizeSm
                                                font.family: PairionStyle.fontFamily
                                                wrapMode: Text.WordWrap
                                            }

                                            Text {
                                                text: modelData.timestamp
                                                color: PairionStyle.dim
                                                opacity: 0.35
                                                font.pixelSize: 8
                                                font.family: PairionStyle.fontFamily
                                            }
                                        }
                                    }
                                }
                            }

                            // Empty turns state
                            Text {
                                width: panel.width - 2
                                horizontalAlignment: Text.AlignHCenter
                                text: "NO TURNS"
                                color: PairionStyle.cyan
                                opacity: 0.3
                                font.pixelSize: PairionStyle.fontSizeSm
                                font.letterSpacing: 1.5
                                font.family: PairionStyle.fontFamily
                                visible: MemoryBrowserModel.selectedEpisodeTurns.length === 0
                            }
                        }
                    }
                }
            }

            // ── Preferences tab ──────────────────────────────────────────────
            Item {
                anchors.fill: parent
                visible: !MemoryBrowserModel.loading &&
                         MemoryBrowserModel.errorMessage === "" &&
                         tabBar.activeTab === 1

                // Empty state
                Text {
                    anchors.centerIn: parent
                    text: "NO PREFERENCES"
                    color: PairionStyle.cyan
                    opacity: 0.3
                    font.pixelSize: PairionStyle.fontSizeSm
                    font.letterSpacing: 1.5
                    font.family: PairionStyle.fontFamily
                    visible: MemoryBrowserModel.preferences.length === 0
                }

                ScrollView {
                    anchors.fill: parent
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    visible: MemoryBrowserModel.preferences.length > 0

                    Column {
                        width: panel.width
                        spacing: 0

                        Repeater {
                            model: MemoryBrowserModel.preferences

                            Rectangle {
                                width: panel.width
                                height: prefCol.height + 16
                                color: "transparent"

                                Rectangle {
                                    anchors.bottom: parent.bottom
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    height: 1
                                    color: PairionStyle.cyan
                                    opacity: 0.1
                                }

                                Column {
                                    id: prefCol
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    spacing: 3

                                    Text {
                                        text: modelData.key
                                        color: PairionStyle.cyan
                                        opacity: 0.7
                                        font.pixelSize: PairionStyle.fontSizeXs
                                        font.letterSpacing: 1.0
                                        font.family: PairionStyle.fontFamily
                                        elide: Text.ElideRight
                                        width: parent.width
                                    }

                                    Text {
                                        text: modelData.value
                                        color: PairionStyle.dim
                                        font.pixelSize: PairionStyle.fontSizeSm
                                        font.family: PairionStyle.fontFamily
                                        wrapMode: Text.WordWrap
                                        width: parent.width
                                    }

                                    Text {
                                        text: modelData.updatedAt
                                        color: PairionStyle.dim
                                        opacity: 0.35
                                        font.pixelSize: 8
                                        font.family: PairionStyle.fontFamily
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ── Auto-refresh on open ─────────────────────────────────────────────────
    onMemoryVisibleChanged: {
        if (memoryVisible)
            MemoryBrowserModel.refresh()
    }
}
