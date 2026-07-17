// Background wallpaper — fills screen on background layer
import Quickshell
import Quickshell.Wayland._WlrLayerShell
import QtQuick
import "../theme"

WlrLayershell {
    id: wallpaper
    // modelData is set by Variants in shell.qml
    property var modelData

    Component.onCompleted: {
        if (modelData) wallpaper.screen = modelData
    }

    anchors {
        top: true
        bottom: true
        left: true
        right: true
    }
    layer: WlrLayer.Background
    keyboardFocus: WlrKeyboardFocus.None
    visible: true

    // Solid fill with subtle gradient
    Rectangle {
        anchors.fill: parent

        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.bg }
            GradientStop { position: 0.5; color: "#1c1c36" }
            GradientStop { position: 1.0; color: Theme.bgAlt }
        }
    }
}
