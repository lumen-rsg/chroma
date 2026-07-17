// System tray — StatusNotifier host
import Quickshell
import Quickshell.Services.SystemTray
import Quickshell.Widgets
import QtQuick
import "../theme"

Row {
    id: root
    spacing: 2

    // SystemTray is a singleton — access directly
    Repeater {
        model: SystemTray.items
        delegate: MouseArea {
            width: Theme.trayIconSize + 6
            height: Theme.barHeight
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor

            Rectangle {
                anchors.fill: parent
                color: parent.containsMouse ? Theme.glassHover : "transparent"
                radius: Theme.radiusSm
                Behavior on color { PropertyAnimation { duration: Theme.animFast } }
            }

            IconImage {
                anchors.centerIn: parent
                width: Theme.trayIconSize
                height: Theme.trayIconSize
                source: modelData.icon
                smooth: true
            }

            // Right-click context menu could go here
            onClicked: {
                // Activate the tray item on left click
                // Most SNI items handle activation internally
            }
        }
    }
}
