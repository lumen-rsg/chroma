// Individual notification popup
import Quickshell
import Quickshell.Wayland._WlrLayerShell
import Quickshell.Widgets
import Quickshell.Services.Notifications
import QtQuick
import QtQuick.Layouts
import "../theme"

WlrLayershell {
    id: popup

    required property var notif

    anchors {
        top: true
        right: true
    }
    anchors.rightMargin: Theme.notifMargin
    layer: WlrLayer.Overlay
    keyboardFocus: WlrKeyboardFocus.None
    visible: true
    implicitWidth: Theme.notifWidth
    implicitHeight: Math.min(popupLayout.implicitHeight + 24, Theme.notifMaxHeight)

    // ── Slide-in animation ───────────────────────────────
    property real slideOffset: -implicitHeight - 20
    NumberAnimation on slideOffset {
        to: 0
        duration: Theme.animNormal
        easing.type: Easing.OutCubic
    }

    transform: Translate {
        y: popup.slideOffset
    }

    // ── Glassmorphism card ───────────────────────────────
    Rectangle {
        anchors.fill: parent
        color: "#ee16213e"
        radius: Theme.radius
        border { color: Theme.glassBorder; width: 1 }

        // Urgency accent strip on left
        Rectangle {
            anchors {
                left: parent.left
                top: parent.top
                bottom: parent.bottom
            }
            width: 3
            radius: 2
            color: {
                if (!popup.notif) return Theme.accentAlt
                switch (popup.notif.urgency) {
                    case NotificationUrgency.Critical: return Theme.accent
                    case NotificationUrgency.Low:     return Theme.fgFaint
                    default:                          return Theme.accentAlt
                }
            }
        }
    }

    ColumnLayout {
        id: popupLayout
        anchors {
            fill: parent
            margins: 12
            leftMargin: 18
        }
        spacing: 6

        // ── Header: app icon + app name + dismiss button ──
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            IconImage {
                Layout.preferredWidth: 20
                Layout.preferredHeight: 20
                source: popup.notif ? (popup.notif.appIcon || "") : ""
                smooth: true
            }

            Text {
                Layout.fillWidth: true
                text: popup.notif ? (popup.notif.appName || "Notification") : ""
                color: Theme.fgDim
                font { family: Theme.fontFamily; pixelSize: Theme.fontSizeSm; weight: Theme.fontWeightBold }
                renderType: Text.NativeRendering
                elide: Text.ElideRight
            }

            Text {
                text: "✕"
                color: Theme.fgFaint
                font.pixelSize: Theme.fontSizeSm
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (popup.notif && popup.notif.tracked) {
                            popup.notif.dismiss()
                        }
                    }
                }
            }
        }

        // ── Summary ──────────────────────────────────────
        Text {
            Layout.fillWidth: true
            text: popup.notif ? (popup.notif.summary || "") : ""
            color: Theme.fg
            font { family: Theme.fontFamily; pixelSize: Theme.fontSize; weight: Theme.fontWeightBold }
            renderType: Text.NativeRendering
            elide: Text.ElideRight
            maximumLineCount: 1
            visible: text !== ""
        }

        // ── Body ─────────────────────────────────────────
        Text {
            Layout.fillWidth: true
            text: popup.notif ? (popup.notif.body || "") : ""
            color: Theme.fgDim
            font { family: Theme.fontFamily; pixelSize: Theme.fontSizeSm }
            renderType: Text.NativeRendering
            wrapMode: Text.WordWrap
            maximumLineCount: 3
            elide: Text.ElideRight
            visible: text !== ""
        }

        // ── Action buttons ──────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: popup.notif && popup.notif.actions && popup.notif.actions.length > 0

            Repeater {
                model: popup.notif ? popup.notif.actions : []
                delegate: Rectangle {
                    height: 26
                    width: actionText.implicitWidth + 16
                    radius: Theme.radiusSm
                    color: actionMouse.containsMouse ? Theme.glassHover : Theme.bgAlt

                    Text {
                        id: actionText
                        anchors.centerIn: parent
                        text: modelData.text || modelData.identifier || ""
                        color: Theme.fg
                        font { family: Theme.fontFamily; pixelSize: Theme.fontSizeSm }
                        renderType: Text.NativeRendering
                    }

                    MouseArea {
                        id: actionMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            modelData.invoke()
                            if (popup.notif && popup.notif.tracked) {
                                popup.notif.dismiss()
                            }
                        }
                    }
                }
            }
        }
    }
}
