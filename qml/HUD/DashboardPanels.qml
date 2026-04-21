/**
 * @file DashboardPanels.qml
 * @brief Four side-by-side data panels: News, Inbox, Todo, Homestead Status.
 */
import QtQuick

Item {
    id: root
    readonly property string mono: "Courier New"
    readonly property color cyan: "#00b4ff"
    readonly property color dim:  "#e0e8f0"

    Row {
        anchors.fill: parent
        spacing: 10

        // ── Panel A: News ─────────────────────────────────────────────────────
        Rectangle {
            width: (parent.width - 30) / 4
            height: parent.height
            color: "#0d1220"
            border.color: root.cyan
            border.width: 1
            radius: 4

            Column {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 5

                Text { text: "NEWS HEADLINES"; color: root.cyan; opacity: 0.7; font.pixelSize: 10; font.letterSpacing: 1.5; font.family: root.mono }
                Rectangle { width: parent.width; height: 1; color: root.cyan; opacity: 0.2 }
                Text { text: "\u2022 Market Volatility and Economic Outlook";  color: root.dim; font.pixelSize: 10; font.family: root.mono; wrapMode: Text.WordWrap; width: parent.width }
                Text { text: "\u2022 Renewable Energy Breakthrough Announced"; color: root.dim; font.pixelSize: 10; font.family: root.mono; wrapMode: Text.WordWrap; width: parent.width }
                Text { text: "\u2022 Cyberattack on Corporate Networks";       color: root.dim; font.pixelSize: 10; font.family: root.mono; wrapMode: Text.WordWrap; width: parent.width }
                Text { text: "\u2022 Leaders: Climate Change Summit";          color: root.dim; font.pixelSize: 10; font.family: root.mono; wrapMode: Text.WordWrap; width: parent.width }
                Text { text: "\u2022 Space Agency: Mars Mission Update";       color: root.dim; font.pixelSize: 10; font.family: root.mono; wrapMode: Text.WordWrap; width: parent.width }
            }
        }

        // ── Panel B: Inbox ────────────────────────────────────────────────────
        Rectangle {
            width: (parent.width - 30) / 4
            height: parent.height
            color: "#0d1220"
            border.color: root.cyan
            border.width: 1
            radius: 4

            Column {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 3

                Text { text: "INBOX"; color: root.cyan; opacity: 0.7; font.pixelSize: 10; font.letterSpacing: 1.5; font.family: root.mono }
                Text { text: "7 UNREAD"; color: "#ff8800"; font.pixelSize: 9; font.family: root.mono }
                Rectangle { width: parent.width; height: 1; color: root.cyan; opacity: 0.2 }
                Text { text: "Jennifer Carter"; color: root.dim; font.pixelSize: 10; font.bold: true; font.family: root.mono; elide: Text.ElideRight; width: parent.width }
                Text { text: "Meeting Update  -12:00"; color: root.dim; opacity: 0.55; font.pixelSize: 9; font.family: root.mono }
                Text { text: "Robert Hughes"; color: root.dim; font.pixelSize: 10; font.bold: true; font.family: root.mono }
                Text { text: "Status Report  -10:00"; color: root.dim; opacity: 0.55; font.pixelSize: 9; font.family: root.mono }
                Text { text: "Amazon"; color: root.dim; font.pixelSize: 10; font.bold: true; font.family: root.mono }
                Text { text: "Delivery Shipped  -8:00"; color: root.dim; opacity: 0.55; font.pixelSize: 9; font.family: root.mono }
                Text { text: "Laura Mitchell"; color: root.dim; font.pixelSize: 10; font.bold: true; font.family: root.mono }
                Text { text: "Project Details  -6:00"; color: root.dim; opacity: 0.55; font.pixelSize: 9; font.family: root.mono }
                Text { text: "John Wong"; color: root.dim; font.pixelSize: 10; font.bold: true; font.family: root.mono }
                Text { text: "Budget Approval  -4:00"; color: root.dim; opacity: 0.55; font.pixelSize: 9; font.family: root.mono }
                Text { text: "John Peterson"; color: root.dim; font.pixelSize: 10; font.bold: true; font.family: root.mono }
                Text { text: "New Proposal  -2:00"; color: root.dim; opacity: 0.55; font.pixelSize: 9; font.family: root.mono }
            }
        }

        // ── Panel C: Todo ─────────────────────────────────────────────────────
        Rectangle {
            width: (parent.width - 30) / 4
            height: parent.height
            color: "#0d1220"
            border.color: root.cyan
            border.width: 1
            radius: 4

            Column {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6

                Text { text: "TODO"; color: root.cyan; opacity: 0.7; font.pixelSize: 10; font.letterSpacing: 1.5; font.family: root.mono }
                Rectangle { width: parent.width; height: 1; color: root.cyan; opacity: 0.2 }
                Text { text: "\u2611 Review quarterly report"; color: root.dim; opacity: 0.35; font.pixelSize: 10; font.family: root.mono; font.strikeout: true; width: parent.width }
                Text { text: "\u2610 Optimize security protocols"; color: root.dim; font.pixelSize: 10; font.family: root.mono; width: parent.width }
                Text { text: "\u2610 Research sustainable operations"; color: root.dim; font.pixelSize: 10; font.family: root.mono; width: parent.width }
                Text { text: "\u2611 Complete vendor assessments"; color: root.dim; opacity: 0.35; font.pixelSize: 10; font.family: root.mono; font.strikeout: true; width: parent.width }
                Text { text: "\u2610 Update team documentation"; color: root.dim; font.pixelSize: 10; font.family: root.mono; width: parent.width }
            }
        }

        // ── Panel D: Homestead ────────────────────────────────────────────────
        Rectangle {
            width: (parent.width - 30) / 4
            height: parent.height
            color: "#0d1220"
            border.color: root.cyan
            border.width: 1
            radius: 4

            Column {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 8

                Text { text: "HOMESTEAD STATUS"; color: root.cyan; opacity: 0.7; font.pixelSize: 10; font.letterSpacing: 1.5; font.family: root.mono }
                Rectangle { width: parent.width; height: 1; color: root.cyan; opacity: 0.2 }
                Text { text: "[W] WEATHER  -  63F Overcast"; color: root.dim; font.pixelSize: 10; font.family: root.mono; width: parent.width }
                Text { text: "[C] CHICKENS  -  8 birds, healthy"; color: root.dim; font.pixelSize: 10; font.family: root.mono; width: parent.width }
                Text { text: "[G] GARDEN  -  Tomatoes, Watered"; color: root.dim; font.pixelSize: 10; font.family: root.mono; width: parent.width }
                Text { text: "[D] DOGS  -  2 dogs, Fed 6PM"; color: root.dim; font.pixelSize: 10; font.family: root.mono; width: parent.width }
            }
        }
    }
}
