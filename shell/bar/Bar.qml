// Top bar panel — one per screen
import Quickshell
import Quickshell.Wayland._WlrLayerShell
import Quickshell.Wayland._BackgroundEffect
import QtQuick
import QtQuick.Layouts
import "../theme"

WlrLayershell {
    id: bar
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

    // ── Compositor-level blur behind the bar ────────────────
    BackgroundEffect.blurRegion: Region {
        item: bar
    }

    // ── Animated content wrapper ────────────────────────────
    // WlrLayershell doesn't support opacity/transform, so we
    // animate an inner Item instead.
    Item {
        id: barContent
        anchors.fill: parent

        // Slide-down entrance
        property real slideY: -Theme.barHeight

        transform: Translate { y: barContent.slideY }

        NumberAnimation on slideY {
            to: 0
            duration: Theme.animSlow
            easing.type: Easing.OutCubic
            running: true
        }

        // ── Glassmorphism bar surface ───────────────────────
        Rectangle {
            id: barSurface
            anchors.fill: parent
            color: Theme.glassBgStrong
            radius: Theme.barRadius

            // Bottom border
            Rectangle {
                anchors {
                    left: parent.left; right: parent.right
                    bottom: parent.bottom
                }
                height: 1
                color: Theme.borderSubtle
            }

            // Top highlight line
            Rectangle {
                anchors {
                    left: parent.left; right: parent.right
                    top: parent.top
                }
                height: 1
                color: "#10ffffff"
            }
        }

        // ── Layout ──────────────────────────────────────────
        RowLayout {
            anchors {
                fill: parent
                leftMargin: Theme.barPadding
                rightMargin: Theme.barPadding
            }
            spacing: Theme.barItemSpacing

            // ── Left: Launcher button ───────────────────────
            Item {
                id: launcherBtn
                Layout.preferredWidth: Theme.barHeight - 4
                Layout.preferredHeight: Theme.barHeight
                Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter

                property bool hovered: false
                property bool pressed: false

                Rectangle {
                    anchors.centerIn: parent
                    width: launcherBtn.width
                    height: parent.height - 8
                    radius: Theme.radiusSm
                    color: launcherBtn.hovered
                        ? (AppState.launcherOpen
                            ? Theme.glassActive : Theme.glassHover)
                        : "transparent"
                    opacity: launcherBtn.hovered
                        || AppState.launcherOpen ? 1 : 0

                    Behavior on color {
                        ColorAnimation { duration: Theme.animFast }
                    }
                    Behavior on opacity {
                        NumberAnimation { duration: Theme.animFast }
                    }
                }

                // 9-dot grid icon
                Canvas {
                    id: gridIcon
                    anchors.centerIn: parent
                    width: 18
                    height: 18
                    scale: launcherBtn.pressed ? 0.85
                        : (launcherBtn.hovered ? 1.12 : 1.0)

                    Behavior on scale {
                        NumberAnimation {
                            duration: Theme.animFast
                            easing.type: Easing.OutBack
                        }
                    }

                    property color dotColor: AppState.launcherOpen
                        ? Theme.accent : Theme.fgDim
                    property real dotSize: 3.5
                    property real spacing: 5.5

                    Behavior on dotColor {
                        ColorAnimation { duration: Theme.animNormal }
                    }

                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        ctx.fillStyle = dotColor
                        for (var row = 0; row < 3; row++) {
                            for (var col = 0; col < 3; col++) {
                                var x = col * (dotSize + spacing)
                                var y = row * (dotSize + spacing)
                                ctx.beginPath()
                                ctx.arc(x + dotSize/2, y + dotSize/2,
                                    dotSize/2, 0, Math.PI * 2)
                                ctx.fill()
                            }
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    onEntered: launcherBtn.hovered = true
                    onExited: {
                        launcherBtn.hovered = false
                        launcherBtn.pressed = false
                    }
                    onPressed: launcherBtn.pressed = true
                    onReleased: launcherBtn.pressed = false
                    onClicked: AppState.launcherOpen
                        = !AppState.launcherOpen
                }
            }

            // ── Center: Window title ────────────────────────
            WindowTitle {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                Layout.preferredHeight: Theme.barHeight
            }

            // ── Right: System tray + clock ──────────────────
            RowLayout {
                Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
                spacing: Theme.barItemSpacing

                SystemTray {
                    Layout.alignment: Qt.AlignVCenter
                }

                Rectangle {
                    width: 3; height: 3
                    radius: 1.5
                    color: Theme.fgFaint
                    Layout.alignment: Qt.AlignVCenter
                    opacity: 0.5
                }

                Clock {
                    Layout.alignment: Qt.AlignVCenter
                }
            }
        }
    }
}
