// Background wallpaper — fills screen on background layer
import Quickshell
import Quickshell.Wayland._WlrLayerShell
import QtQuick
import "../theme"

WlrLayershell {
    id: wallpaper
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

    // Performance: tell compositor this surface is opaque
    surfaceFormat.opaque: true
    color: Theme.bg

    // ── Multi-stop gradient background ──────────────────────
    Rectangle {
        anchors.fill: parent

        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0.0; color: Theme.bg }
            GradientStop { position: 0.3; color: Qt.lighter(Theme.bg, 1.04) }
            GradientStop { position: 0.6; color: Theme.bgAlt }
            GradientStop { position: 1.0; color: Qt.darker(Theme.bgAlt, 1.15) }
        }
    }

    // ── Subtle vignette overlay ─────────────────────────────
    Rectangle {
        anchors.fill: parent

        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0.0; color: "#00000000" }
            GradientStop { position: 0.70; color: "#00000000" }
            GradientStop { position: 1.0; color: "#18000000" }
        }
    }

    // ── Subtle radial highlight (center glow) ───────────────
    Rectangle {
        anchors.fill: parent

        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0.0; color: "#08ffffff" }
            GradientStop { position: 0.40; color: "#04ffffff" }
            GradientStop { position: 1.0; color: "#00000000" }
        }
    }
}
