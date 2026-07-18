// Active window title display with app icon
import Quickshell
import Quickshell.Wayland._ToplevelManagement
import Quickshell.Widgets
import QtQuick
import "../theme"

Item {
    id: root
    implicitHeight: Theme.barHeight

    // ── Centered content row ────────────────────────────────
    Row {
        id: titleRow
        anchors.centerIn: parent
        spacing: 6
        width: Math.min(implicitWidth, parent.width - 20)

        // ── App icon ────────────────────────────────────────
        IconImage {
            id: appIcon
            anchors.verticalCenter: parent.verticalCenter
            width: Theme.iconSizeSm
            height: Theme.iconSizeSm
            source: ToplevelManager.activeToplevel
                ? (ToplevelManager.activeToplevel.icon || "")
                : ""
            smooth: true
            visible: source != ""
            opacity: ToplevelManager.activeToplevel ? 0.7 : 0.3

            Behavior on opacity {
                NumberAnimation { duration: Theme.animFast; easing.type: Easing.OutCubic }
            }
        }

        // ── Title text ──────────────────────────────────────
        Text {
            id: titleText
            anchors.verticalCenter: parent.verticalCenter

            property string activeTitle: ToplevelManager.activeToplevel
                ? ToplevelManager.activeToplevel.title
                : ""

            text: activeTitle !== "" ? activeTitle : "—"
            color: activeTitle !== "" ? Theme.fg : Theme.fgFaint
            font {
                family: Theme.fontFamily
                pixelSize: Theme.fontSizeSm
                weight: Theme.fontWeightMedium
            }
            renderType: Text.NativeRendering
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            maximumLineCount: 1

            // Smooth color transition on focus change
            Behavior on color {
                ColorAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic }
            }
        }
    }
}
