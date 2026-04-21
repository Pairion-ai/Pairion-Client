/**
 * @file HudLayout.qml
 * @brief Full 2560×1440 panel layout for the Pairion cinematic HUD.
 *
 * Positions all dashboard panels relative to the parent (full-screen Item).
 * The ring system is NOT rendered here — it remains in PairionHUD.qml centered
 * behind this layout. HudLayout sits on top and fills the parent; all panels
 * use transparent/translucent backgrounds so the rings show through the center.
 *
 * Panel map:
 *   Top row:    Western Hemisphere map (left) | Eastern Hemisphere map (right)
 *   Bottom row: INBOX | NEWS HEADLINES | TODO | HOMESTEAD STATUS
 *   Footer:     TRANSCRIPT STRIP (full width)
 */

import QtQuick
import Pairion

Item {
    id: root

    /**
     * @brief HUD state string forwarded from PairionHUD for panel color accents.
     */
    property string hudState: "idle"

    // ── Layout constants ──────────────────────────────────────────────────────

    readonly property int margin:       24   // outer edge margin
    readonly property int gap:          20   // inter-panel gap
    readonly property int mapW:        450   // hemisphere map width
    readonly property int mapH:        350   // hemisphere map height
    readonly property int bottomH:     250   // bottom panel height
    readonly property int transcriptH:  80   // transcript strip height
    readonly property int headerH:      26   // panel header height (DashboardPanel internal)

    // Vertical centre of the screen — rings are centred here.
    // Maps are vertically centred in the top 60% of the screen.
    readonly property real topZoneH:   root.height - bottomH - transcriptH - gap * 2 - margin
    readonly property real mapTopY:    (topZoneH - mapH) * 0.5

    // Bottom row top edge
    readonly property real bottomRowY: root.height - transcriptH - gap - bottomH - margin

    // Bottom panels share width equally across 4 columns
    readonly property real bottomPanelW: (root.width - margin * 2 - gap * 3) / 4

    // ── WESTERN HEMISPHERE MAP ────────────────────────────────────────────────

    DashboardPanel {
        id: westPanel
        x:      root.margin
        y:      root.mapTopY
        width:  root.mapW
        height: root.mapH
        title:  "Western Hemisphere"

        HemisphereMap {
            anchors.fill:    parent
            anchors.topMargin: 22   // clear the DashboardPanel header
            hemisphere:      "western"
        }
    }

    // ── EASTERN HEMISPHERE MAP ────────────────────────────────────────────────

    DashboardPanel {
        id: eastPanel
        x:      root.width - root.margin - root.mapW
        y:      root.mapTopY
        width:  root.mapW
        height: root.mapH
        title:  "Eastern Hemisphere"

        HemisphereMap {
            anchors.fill:    parent
            anchors.topMargin: 22
            hemisphere:      "eastern"
        }
    }

    // ── BOTTOM PANELS ─────────────────────────────────────────────────────────

    DashboardPanel {
        id: inboxPanel
        x:      root.margin
        y:      root.bottomRowY
        width:  root.bottomPanelW
        height: root.bottomH
        title:  "Inbox"
    }

    DashboardPanel {
        id: newsPanel
        x:      root.margin + root.bottomPanelW + root.gap
        y:      root.bottomRowY
        width:  root.bottomPanelW
        height: root.bottomH
        title:  "News Headlines"
    }

    DashboardPanel {
        id: todoPanel
        x:      root.margin + (root.bottomPanelW + root.gap) * 2
        y:      root.bottomRowY
        width:  root.bottomPanelW
        height: root.bottomH
        title:  "Todo"
    }

    DashboardPanel {
        id: homesteadPanel
        x:      root.margin + (root.bottomPanelW + root.gap) * 3
        y:      root.bottomRowY
        width:  root.bottomPanelW
        height: root.bottomH
        title:  "Homestead Status"
    }

    // ── TRANSCRIPT STRIP ──────────────────────────────────────────────────────

    DashboardPanel {
        id: transcriptPanel
        x:      root.margin
        y:      root.height - root.margin - root.transcriptH
        width:  root.width - root.margin * 2
        height: root.transcriptH
        title:  "Transcript"

        Text {
            anchors.fill:        parent
            anchors.leftMargin:  4
            verticalAlignment:   Text.AlignVCenter
            text:                ConnectionState.transcriptFinal !== ""
                                     ? ConnectionState.transcriptFinal
                                     : ConnectionState.transcriptPartial
            color:               "#e0e8f0"
            opacity:             0.80
            font.pixelSize:      12
            font.family:         "Courier New"
            elide:               Text.ElideRight
            wrapMode:            Text.NoWrap
        }
    }
}
