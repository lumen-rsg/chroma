// System tray — StatusNotifier host
import Quickshell
import Quickshell.Services.SystemTray
import Quickshell.Widgets
import QtQuick
import "../theme"

Row {
    id: root
    spacing: 1

    Repeater {
        model: SystemTray.items
        delegate: Item {
            id: trayItem
            width: Theme.trayIconSize + 12
            height: Theme.barHeight

            property bool hovered: false
            property bool pressed: false

            // ── Hover pill background ──────────────────────
            Rectangle {
                anchors.centerIn: parent
                width: trayItem.width - 4
                height: parent.height - 10
                radius: Theme.radiusSm
                color: trayItem.hovered ? Theme.glassHover : "transparent"
                opacity: trayItem.hovered ? 1 : 0

                Behavior on color { ColorAnimation { duration: Theme.animFast } }
                Behavior on opacity { NumberAnimation { duration: Theme.animFast } }
            }

            // ── Icon with scale feedback ───────────────────
            IconImage {
                id: trayIcon
                anchors.centerIn: parent
                width: Theme.trayIconSize
                height: Theme.trayIconSize
                source: modelData.icon || ""
                smooth: true
                scale: trayItem.pressed ? 0.8 : (trayItem.hovered ? 1.15 : 1.0)

                Behavior on scale {
                    NumberAnimation {
                        duration: Theme.animFast
                        easing.type: Easing.OutBack
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onEntered: trayItem.hovered = true
                onExited: { trayItem.hovered = false; trayItem.pressed = false }
                onPressed: trayItem.pressed = true
                onReleased: trayItem.pressed = false
                onClicked: {
                    // Left-click activates the tray item
                    // Most SNI items handle activation internally
                }
                // Right-click for context menu could be added here
            }
        }
    }
}
