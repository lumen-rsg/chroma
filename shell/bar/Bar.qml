// Top bar panel — one per screen
import Quickshell
import Quickshell.Wayland._WlrLayerShell
import QtQuick
import QtQuick.Layouts
import "../theme"

WlrLayershell {
    id: bar
    // modelData is set by Variants in shell.qml
    property var modelData

    Component.onCompleted: {
        if (modelData) bar.screen = modelData
    }

    anchors {
        top: true
        left: true
        right: true
    }
    layer: WlrLayer.Top
    keyboardFocus: WlrKeyboardFocus.None
    implicitHeight: Theme.barHeight
    exclusiveZone: Theme.barHeight
    exclusionMode: ExclusionMode.Normal

    // ── Glassmorphism background ──────────────────────────
    Rectangle {
        anchors.fill: parent
        color: Theme.glassBg
        radius: 0

        // Bottom border / separator
        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: 1
            color: Theme.glassBorder
        }
    }

    // ── Layout ────────────────────────────────────────────
    RowLayout {
        anchors {
            fill: parent
            leftMargin: Theme.barPadding
            rightMargin: Theme.barPadding
        }
        spacing: Theme.barPadding

        // Left: App launcher button
        Item {
            Layout.preferredWidth: 28
            Layout.preferredHeight: Theme.barHeight
            Layout.alignment: Qt.AlignLeft

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                hoverEnabled: true

                Rectangle {
                    anchors.fill: parent
                    color: parent.containsMouse ? Theme.glassHover : "transparent"
                    radius: Theme.radiusSm
                    Behavior on color { PropertyAnimation { duration: Theme.animFast } }
                }

                Text {
                    anchors.centerIn: parent
                    text: "✦"
                    color: parent.parent.containsMouse ? Theme.accent : Theme.fgDim
                    font { family: Theme.fontFamily; pixelSize: Theme.fontSizeLg }
                    Behavior on color { PropertyAnimation { duration: Theme.animFast } }
                }

                onClicked: AppState.launcherOpen = !AppState.launcherOpen
            }
        }

        // Center: Window title
        WindowTitle {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredHeight: Theme.barHeight
        }

        // Right: System tray + clock
        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: Theme.barPadding

            SystemTray {
                Layout.alignment: Qt.AlignVCenter
            }

            Rectangle {
                width: 1
                height: Theme.barHeight - 12
                color: Theme.fgFaint
                Layout.alignment: Qt.AlignVCenter
            }

            Clock {
                Layout.alignment: Qt.AlignVCenter
            }
        }
    }
}
