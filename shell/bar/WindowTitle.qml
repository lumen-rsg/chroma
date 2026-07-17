// Active window title display
import Quickshell
import Quickshell.Wayland._ToplevelManagement
import QtQuick
import "../theme"

Text {
    id: root

    property string activeTitle: ToplevelManager.activeToplevel
        ? ToplevelManager.activeToplevel.title
        : ""

    text: activeTitle !== "" ? activeTitle : "Chroma"
    color: activeTitle !== "" ? Theme.fg : Theme.fgDim
    font { family: Theme.fontFamily; pixelSize: Theme.fontSize; weight: Theme.fontWeightNormal }
    renderType: Text.NativeRendering
    elide: Text.ElideRight
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter

    Behavior on text {
        SequentialAnimation {
            PropertyAnimation { target: root; property: "opacity"; to: 0; duration: 100 }
            PropertyAction  { target: root; property: "text" }
            PropertyAnimation { target: root; property: "opacity"; to: 1; duration: 100 }
        }
    }
}
